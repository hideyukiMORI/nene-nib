// scope `--text-buffer-property` の単体テスト（#869・ADR 0042 決定 2・ADR 0044 / 0047 / 0121 /
// 0139 / 0052）。固定 seed の決定的な編集の列（insert / erase / replaced と範囲の外の端）を
// TextBuffer と std::string の模型の両方に施し、各段で本文・行・位置・仮想桁を模型の素朴な走査と
// 照合する。最後に EditHistory の undo / redo を全段ぶん往復させる。
// 模型の行は TextBuffer.hpp の注釈に従う: 行は '\n' で切り、内容の終わりから '\r' を 1 つ外すのは
// 改行の形が CRLF の文書だけ（LF の文書では '\r' は 1 文字）。CRLF の文書で '\n' の続かない最終行の
// 末尾の '\r' も外すのは注釈に無く、既存の試験（"the existing CRLF model also strips a terminal CR
// without a following LF"）から読んだ規則である。
#include "Column.hpp"
#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "EditHistory.hpp"
#include "HistoryFailure.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "VirtualColumn.hpp"
#include "VirtualColumnIndex.hpp"
#include "VirtualColumnsFailure.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::Column;
using core::column_advance;
using core::detect_line_ending;
using core::Edit;
using core::EditBoundary;
using core::EditHistory;
using core::HistoryFailure;
using core::LineEnding;
using core::LineNumber;
using core::Offset;
using core::TextBuffer;
using core::TextPosition;
using core::VirtualColumnIndex;
using core::VirtualColumnsFailure;

using Random = std::mt19937_64;
// 本文の 1 つの境目で模型が答える値: バイト位置・行（1 始まり）・行頭からの序数・仮想桁。
using Walked = std::array<std::size_t, 4>;
// 模型の 1 行: 行頭・内容の終わり・次の行の先頭（最終行では本文の末尾）。
using ModelLine = std::array<std::size_t, 3>;
// 編集の形: 範囲が跨ぐ境目の数の上限・入れる断片の数の上限・段を足し続ける挿入の合計の下限。
using EditShape = std::array<std::size_t, 3>;
// 1 段の編集と、それを TextBuffer へ施す道（edited の注釈）。
using PlannedEdit = std::pair<Edit, std::size_t>;
// 索引を持たない本文と持つ本文の組。同じ編集の列で動かす。
using Buffers = std::pair<TextBuffer, TextBuffer>;

constexpr std::size_t at_offset = 0;
constexpr std::size_t at_line = 1;
constexpr std::size_t at_code_point = 2;
constexpr std::size_t at_column = 3;
constexpr std::size_t line_begin = 0;
constexpr std::size_t line_content_end = 1;
constexpr std::size_t line_next = 2;

// Debug で 1 秒以内に収まる回数（#869）。50 倍の走りはこの 3 つを掛けて手元で回し、commit しない。
constexpr std::uint64_t property_seed = 0x6E69'6238'3639ULL;
constexpr std::size_t small_cases = 12;
constexpr std::size_t small_steps = 20;
constexpr std::size_t crossing_steps = 5;

// 本文の断片。ASCII・Tab・LF・CRLF・孤立した CR・2〜4 バイトの UTF-8・結合文字 U+0301・全角・
// 絵文字・幅 0 の書式用文字 U+200B・C1 の U+0085・制御文字・空を混ぜる。
constexpr std::array<std::string_view, 15> fragments{{
    "a",
    "Z",
    " ",
    "\t",
    "\n",
    "\r\n",
    "\r",
    "\xc3\xa9",
    "e\xcc\x81",
    "\xe6\x97\xa5",
    "\xf0\x9f\x98\x80",
    "\xe2\x80\x8b",
    "\xc2\x85",
    "\x01",
    "",
}};

[[nodiscard]] constexpr std::size_t longest_fragment_bytes()
{
    std::size_t longest = 0;
    for (const auto fragment : fragments)
    {
        longest = std::max(longest, fragment.size());
    }
    return longest;
}

// 塊をまたぐ case の挿入 1 回の上限。仮想桁の索引の塊（約 64 KiB）より小さく保ち、
// 1 回の編集が触れる塊を隣り合う 2 つまでにする（verify_columns の 3 塊までの根拠）。
// 上限は断片の数で決める（最長の断片 × 数 ≤ 上限）。
constexpr std::size_t crossing_insert_bytes = 32U * 1024U;
constexpr std::size_t crossing_fragments = crossing_insert_bytes / longest_fragment_bytes();
static_assert(crossing_fragments * longest_fragment_bytes() <= crossing_insert_bytes);

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return bound == 0 ? 0 : static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] std::string generated(Random &random, std::size_t count)
{
    std::string text;
    for (std::size_t index = 0; index < count; ++index)
    {
        text += fragments.at(below(random, fragments.size()));
    }
    return text;
}

// 妥当な UTF-8 の 1 つの code point と、そのバイト数。
[[nodiscard]] std::pair<char32_t, std::size_t> decoded_at(std::string_view text, std::size_t at)
{
    const auto lead = static_cast<unsigned char>(text[at]);
    const std::size_t length = lead < 0x80U ? 1 : (lead < 0xE0U ? 2 : (lead < 0xF0U ? 3 : 4));
    char32_t value = length == 1 ? lead : (lead & (0x7FU >> length));
    for (std::size_t index = 1; index < length; ++index)
    {
        value = (value << 6U) | (static_cast<unsigned char>(text[at + index]) & 0x3FU);
    }
    return {value, length};
}

// 模型の素朴な走査。全ての code point の先頭と本文の末尾で、行・行頭からの序数・仮想桁を数える。
// 仮想桁は行頭から column_advance を 1 文字ずつ足す（'\r' も 1 文字として足す）。
[[nodiscard]] std::vector<Walked> walked(std::string_view text)
{
    std::vector<Walked> result;
    Walked state{0, 1, 0, 0};
    while (true)
    {
        result.push_back(state);
        if (state.at(at_offset) == text.size())
        {
            return result;
        }
        const auto [value, length] = decoded_at(text, state.at(at_offset));
        const bool newline = value == U'\n';
        state.at(at_line) += newline ? 1 : 0;
        state.at(at_column) =
            newline ? 0 : state.at(at_column) + column_advance(value, state.at(at_column));
        state.at(at_code_point) = newline ? 0 : state.at(at_code_point) + 1;
        state.at(at_offset) += length;
    }
}

[[nodiscard]] std::vector<ModelLine> model_lines(std::string_view text, LineEnding ending)
{
    std::vector<ModelLine> lines;
    std::size_t start = 0;
    while (true)
    {
        const auto newline = text.find('\n', start);
        const auto stop = newline == std::string_view::npos ? text.size() : newline;
        const bool strip = ending == LineEnding::crlf && stop > start && text[stop - 1] == '\r';
        lines.push_back({start, strip ? stop - 1 : stop,
                         newline == std::string_view::npos ? text.size() : newline + 1});
        if (newline == std::string_view::npos)
        {
            return lines;
        }
        start = newline + 1;
    }
}

// 大きな本文では境目と行を間引く（小さな本文は全部を見る）。
[[nodiscard]] std::size_t stride_of(std::size_t count, std::size_t limit)
{
    return std::max<std::size_t>(1, count / limit);
}

// 1. 本文・大きさ・text_range・segments の連結。
void verify_contents(const TextBuffer &text, std::string_view model,
                     const std::vector<Walked> &walk, Random &random)
{
    std::string joined;
    for (const auto segment : text.segments())
    {
        joined += segment;
    }
    expect(text.text() == model && text.size_bytes() == model.size() && joined == model,
           "the text, its size and its joined segments match the model");
    bool ranges = true;
    for (std::size_t pick = 0; pick < 16; ++pick)
    {
        const auto first = walk.at(below(random, walk.size())).at(at_offset);
        const auto second = walk.at(below(random, walk.size())).at(at_offset);
        const auto low = std::min(first, second);
        const auto high = std::max(first, second);
        ranges =
            ranges && text.text_range(Offset{low}, Offset{high}) == model.substr(low, high - low);
    }
    expect(ranges, "text_range between code point boundaries matches the model's substr");
}

// 2. 行の数・端・文字列と、桁の端（0 と 1 は行頭・行より長い桁は内容の終わり）。
void verify_lines(const TextBuffer &text, std::string_view model,
                  const std::vector<ModelLine> &lines)
{
    expect(text.line_count() == lines.size(), "line_count matches the model's lines");
    // 最長の行は LF を含まず CR は数える（TextBuffer.hpp の注釈）。LF で終わる行は 1 を引く。
    std::size_t longest = 0;
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        const auto &expected = lines.at(index);
        const std::size_t newline = index + 1 < lines.size() ? 1 : 0;
        longest = std::max(longest, expected.at(line_next) - expected.at(line_begin) - newline);
    }
    expect(text.longest_line_bytes() == longest,
           "longest_line_bytes matches the model's longest line without its LF");
    bool ends = true;
    bool columns = true;
    const auto stride = stride_of(lines.size(), 128);
    for (std::size_t index = 0; index < lines.size(); index += stride)
    {
        const LineNumber line{index + 1};
        const auto &expected = lines.at(index);
        const auto content = expected.at(line_content_end) - expected.at(line_begin);
        ends = ends && text.line_start(line).value == expected.at(line_begin) &&
               text.line_end(line).value == expected.at(line_content_end) &&
               text.line_terminator_end(line).value == expected.at(line_next) &&
               text.line_text(line) == model.substr(expected.at(line_begin), content);
        columns = columns &&
                  text.offset_of(TextPosition{line, Column{0}}).value == expected.at(line_begin) &&
                  text.offset_of(TextPosition{line, Column{1}}).value == expected.at(line_begin) &&
                  text.offset_of(TextPosition{line, Column{content + 99}}).value ==
                      expected.at(line_content_end);
    }
    expect(ends, "line_start, line_end, line_terminator_end and line_text match the model");
    expect(columns, "columns 0 and 1 reach the line start and a long column the content end");
}

// 3. 位置の往復。position_of は行頭からの code point の序数＋1 で、offset_of はそれを内容の
// 終わりへ丸めて戻す（CRLF の '\r' と '\n' の間の境目は内容の終わりへ戻る）。
void verify_positions(const TextBuffer &text, const std::vector<Walked> &walk,
                      const std::vector<ModelLine> &lines)
{
    bool forward = true;
    bool back = true;
    const auto stride = stride_of(walk.size(), 128);
    for (std::size_t index = 0; index < walk.size(); index += stride)
    {
        const auto &point = walk.at(index);
        const TextPosition expected{LineNumber{point.at(at_line)},
                                    Column{point.at(at_code_point) + 1}};
        const Offset at{point.at(at_offset)};
        forward =
            forward && text.position_of(at) == expected && text.position_read(at).first == expected;
        const auto content_end = lines.at(point.at(at_line) - 1).at(line_content_end);
        const auto reached = std::min(point.at(at_offset), content_end);
        back = back && text.offset_of(expected).value == reached &&
               text.offset_read(expected).first.value == reached;
    }
    expect(forward, "position_of and position_read count the line and the code points before");
    expect(back, "offset_of and offset_read return the position to its offset");
}

// 行の中で累積の仮想桁 column を占める文字の先頭（幅 0 の文字は占めない）。無ければ内容の終わり。
[[nodiscard]] std::size_t column_target(std::string_view model, const ModelLine &line,
                                        std::size_t column)
{
    std::size_t at = line.at(line_begin);
    std::size_t sum = 0;
    while (at < line.at(line_content_end))
    {
        const auto [value, length] = decoded_at(model, at);
        const auto advance = column_advance(value, sum);
        if (advance > 0 && column < sum + advance)
        {
            return at;
        }
        sum += advance;
        at += length;
    }
    return line.at(line_content_end);
}

// 行の中で ordinal 番目（0 始まり）の code point の先頭。無ければ内容の終わり。
[[nodiscard]] std::size_t code_point_target(std::string_view model, const ModelLine &line,
                                            std::size_t ordinal)
{
    std::size_t at = line.at(line_begin);
    for (std::size_t count = 0; count < ordinal && at < line.at(line_content_end); ++count)
    {
        at += decoded_at(model, at).second;
    }
    return std::min(at, line.at(line_content_end));
}

// 4a. 全ての境目で、索引の仮想桁と序数が行頭から column_advance を素朴に足した値と一致する。
void verify_column_walk(const TextBuffer &text, const VirtualColumnIndex &index,
                        const std::vector<Walked> &walk)
{
    bool same = true;
    const auto stride = stride_of(walk.size(), 128);
    for (std::size_t position = 0; position < walk.size(); position += stride)
    {
        const auto &point = walk.at(position);
        const Offset at{point.at(at_offset)};
        same = same && index.virtual_column_at(text, at) == point.at(at_column) &&
               index.code_point_at(text, at) == point.at(at_code_point);
    }
    expect(same, "the virtual column index answers the naive column_advance sums");
}

// 4b. 行ごとの逆引き（仮想桁 → バイト・序数 → バイト）が模型の素朴な歩きと一致する。
void verify_column_lines(const TextBuffer &text, const VirtualColumnIndex &index,
                         std::string_view model, const std::vector<ModelLine> &lines)
{
    bool columns = true;
    bool ordinals = true;
    const auto stride = stride_of(lines.size(), 24);
    for (std::size_t number = 0; number < lines.size(); number += stride)
    {
        const auto &line = lines.at(number);
        const LineNumber name{number + 1};
        const auto width = walked(model.substr(line.at(line_begin),
                                               line.at(line_content_end) - line.at(line_begin)))
                               .back();
        for (std::size_t column = 0; column <= width.at(at_column) + 2; ++column)
        {
            columns = columns && index.byte_at_virtual_column(text, name, column).value ==
                                     column_target(model, line, column);
        }
        for (std::size_t ordinal = 0; ordinal <= width.at(at_code_point) + 1; ++ordinal)
        {
            ordinals = ordinals && index.byte_at_code_point(text, name, ordinal).value ==
                                       code_point_target(model, line, ordinal);
        }
    }
    expect(columns, "byte_at_virtual_column lands where the naive walk lands");
    expect(ordinals, "byte_at_code_point lands where the naive walk lands");
}

// 4. 索引を持つ本文の仮想桁。replaced の後の読み直しの数は索引の数と同じで、塊の数を超えない。
// 局所の編集（範囲と挿入が塊より十分に小さい）が読み直すのは触れた塊と合わせる隣の 3 つまで。
void verify_columns(const TextBuffer &text, std::string_view model, const std::vector<Walked> &walk,
                    const std::vector<ModelLine> &lines)
{
    const auto held = text.virtual_columns();
    expect(text.has_virtual_columns() && held.has_value(), "the indexed text keeps its index");
    if (!held.has_value())
    {
        return;
    }
    const VirtualColumnIndex &index = held.value().get();
    expect(text.rebuilt_column_chunks() == index.rebuilt_chunks() &&
               index.rebuilt_chunks() <= index.chunk_count() && index.rebuilt_chunks() <= 3,
           "a local edit reads back only the chunks it touched");
    verify_column_walk(text, index, walk);
    verify_column_lines(text, index, model, lines);
}

void verify_step(const Buffers &buffers, std::string_view model, const std::vector<Walked> &walk,
                 Random &random)
{
    const auto &[plain, indexed] = buffers;
    const auto lines = model_lines(model, plain.line_ending());
    verify_contents(plain, model, walk, random);
    verify_contents(indexed, model, walk, random);
    verify_lines(plain, model, lines);
    verify_lines(indexed, model, lines);
    verify_positions(plain, walk, lines);
    verify_positions(indexed, walk, lines);
    verify_columns(indexed, model, walk, lines);
}

// 1 段の編集。道は 0: 範囲の外の begin（size より後ろ）と size より前の end を丸める replaced、
// 1: begin より前の end を丸める replaced、2〜6: insert、7〜10: erase、ほか: replaced。
// 範囲の端は code point の境目で、跨ぐ境目の数は shape の 0 番まで。
[[nodiscard]] PlannedEdit edited(Random &random, const std::string &model,
                                 const std::vector<Walked> &walk, EditShape shape)
{
    const auto first = below(random, walk.size());
    const auto last = std::min(walk.size() - 1, first + below(random, shape.at(0) + 1));
    const auto begin = walk.at(first).at(at_offset);
    const auto end = walk.at(last).at(at_offset);
    auto inserted = generated(random, below(random, shape.at(1) + 1));
    const auto path = below(random, 16);
    if (path == 0)
    {
        return {Edit{Offset{model.size()}, "", inserted, Offset{model.size()}}, path};
    }
    if (path <= 6)
    {
        return {Edit{Offset{begin}, "", inserted, Offset{begin}}, path};
    }
    if (path <= 10)
    {
        inserted.clear();
    }
    return {Edit{Offset{begin}, model.substr(begin, end - begin), inserted, Offset{begin}}, path};
}

[[nodiscard]] TextBuffer applied_edit(const TextBuffer &text, const PlannedEdit &planned)
{
    const auto &[edit, path] = planned;
    const Offset end{edit.at.value + edit.removed.size()};
    if (path == 0)
    {
        return text.replaced(Offset{edit.at.value + 1}, Offset{0}, edit.inserted);
    }
    if (path == 1)
    {
        return text.replaced(edit.at, Offset{edit.at.value == 0 ? 0 : edit.at.value - 1},
                             edit.inserted);
    }
    if (path <= 6)
    {
        return text.insert(edit.at, edit.inserted);
    }
    if (path <= 10)
    {
        return text.erase(edit.at, end);
    }
    return text.replaced(edit.at, end, edit.inserted);
}

// 5a. undo を全段ぶんで最初の本文へ、各段で 1 つ前の本文に戻り、改訂番号は進むだけ。
[[nodiscard]] std::pair<TextBuffer, EditHistory>
undone_all(TextBuffer text, EditHistory history, const std::vector<std::string> &snapshots)
{
    bool same = history.size() + 1 == snapshots.size() && history.position() == history.size();
    for (std::size_t remaining = history.size(); remaining > 0 && same; --remaining)
    {
        const auto edit = history.undo();
        same = edit.has_value();
        if (!same)
        {
            break;
        }
        const auto &value = edit.value();
        text =
            text.replaced(value.at, Offset{value.at.value + value.inserted.size()}, value.removed);
        const auto before = history.revision();
        history = history.undone();
        same = text.text() == snapshots.at(remaining - 1) && history.revision() > before;
    }
    expect(same && history.position() == 0 && !history.undo().has_value() &&
               history.undo().error() == HistoryFailure::nothing_to_undo,
           "undoing every edit walks back through each text to the first one");
    return {text, history};
}

// 5b. redo を全段ぶんで最後の本文へ。
void verify_history_round_trip(const TextBuffer &last, const EditHistory &pushed,
                               const std::vector<std::string> &snapshots)
{
    auto [text, history] = undone_all(last, pushed, snapshots);
    bool same = true;
    for (std::size_t step = 1; step < snapshots.size() && same; ++step)
    {
        const auto edit = history.redo();
        same = edit.has_value();
        if (!same)
        {
            break;
        }
        const auto &value = edit.value();
        text =
            text.replaced(value.at, Offset{value.at.value + value.removed.size()}, value.inserted);
        const auto before = history.revision();
        history = history.redone();
        same = text.text() == snapshots.at(step) && history.revision() > before;
    }
    expect(same && history.position() == history.size() && !history.redo().has_value() &&
               history.redo().error() == HistoryFailure::nothing_to_redo,
           "redoing every edit walks forward through each text to the last one");
}

// 1 つの case。持たない本文と持つ本文を同じ列で動かし、各段で照合する。段は steps 回と、挿入の
// 合計が shape の下限を越えるまで。挿入したバイト数を返す。
std::size_t run_case(Random &random, const std::string &initial, std::size_t steps, EditShape shape)
{
    auto plain = buffer_of(initial);
    Buffers buffers{plain, plain.with_virtual_columns()};
    expect(!plain.has_virtual_columns() && !plain.virtual_columns().has_value() &&
               plain.virtual_columns().error() == VirtualColumnsFailure::not_held,
           "a text without the index answers not_held");
    const auto ending = detect_line_ending(initial);
    std::string model = initial;
    auto history = EditHistory::empty();
    std::vector<std::string> snapshots{model};
    std::size_t inserted = 0;
    auto walk = walked(model);
    verify_step(buffers, model, walk, random);
    for (std::size_t step = 0; step < steps || inserted < shape.at(2); ++step)
    {
        const auto planned = edited(random, model, walk, shape);
        const auto &edit = planned.first;
        buffers =
            Buffers{applied_edit(buffers.first, planned), applied_edit(buffers.second, planned)};
        model.replace(edit.at.value, edit.removed.size(), edit.inserted);
        inserted += edit.inserted.size();
        const auto before = history.revision();
        history = history.pushed(edit, EditBoundary::separate);
        snapshots.push_back(model);
        expect(buffers.first.line_ending() == ending && buffers.second.line_ending() == ending &&
                   history.revision() > before,
               "edits keep the line ending and move the revision");
        walk = walked(model);
        verify_step(buffers, model, walk, random);
    }
    verify_history_round_trip(buffers.first, history, snapshots);
    return inserted;
}

// 手で選んだ端の本文（CRLF の終端の '\r'・'\r\r\n'・LF の孤立した '\r'・空）を同じ照合に通す。
void verify_edge_texts(Random &random)
{
    constexpr std::array<std::string_view, 12> texts{{
        "",
        "\r",
        "\n",
        "\r\n",
        "a\r\nb\r",
        "a\r\r\n\r",
        "\r\n\r\n\r\r",
        "a\nb\r",
        "a\nb\r\nc\r",
        "\n\r\n\r",
        "\t\xe6\x97\xa5"
        "e\xcc\x81\r\n\xe2\x80\x8b\t",
        "\xf0\x9f\x98\x80\t\xc2\x85\x01\n",
    }};
    for (const auto text : texts)
    {
        run_case(random, std::string{text}, 6, EditShape{4, 3, 0});
    }
}

void verify_small_cases(Random &random)
{
    for (std::size_t index = 0; index < small_cases; ++index)
    {
        run_case(random, generated(random, below(random, 48)), small_steps, EditShape{24, 8, 0});
    }
}

// 64 KiB を越える本文（plain の ASCII の行と混ぜた尻尾）に、合わせて 64 KiB を越える挿入を施す。
// 挿入 1 回は crossing_insert_bytes まで（局所の編集）で、合計が 64 KiB を越えるまで段を足す。
// 仮想桁の索引は複数の塊になり、add の chunk の境目も 1 度跨ぐ（ADR 0044 / 0140）。
void verify_chunk_crossing_case(Random &random)
{
    std::string initial;
    while (initial.size() < 72U * 1024U)
    {
        initial += std::string(59, static_cast<char>('a' + (initial.size() % 26)));
        initial += '\n';
    }
    initial += generated(random, 1024);
    const auto inserted = run_case(random, initial, crossing_steps,
                                   EditShape{48, crossing_fragments, (64U * 1024U) + 1U});
    expect(inserted > 64U * 1024U, "the crossing case inserts more than one add chunk");
}
} // namespace

void verify_text_buffer_property_scope()
{
    Random random{property_seed};
    verify_edge_texts(random);
    verify_small_cases(random);
    verify_chunk_crossing_case(random);
}
} // namespace nenenib::tests
