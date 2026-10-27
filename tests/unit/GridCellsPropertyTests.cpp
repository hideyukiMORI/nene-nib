// scope `--grid-cells-property` の性質の試験（#936・ADR 0140 の決定 4・5・ADR 0139・ADR 0034・
// ADR 0042）。固定 seed の決定的な生成器で本文（ASCII・Tab・全角・絵文字・結合文字などの幅 0・
// 書式用文字 U+200B・C1 の U+0085・制御文字・孤立した CR・改行 LF / CRLF・1 行が 1〜20,000
// コードポイント・塊の境目 64 KiB をまたぐ本文）と幅 columns（1〜240）を作り、次を確かめる。
//   1. for_grid の答え（cursor_at の桁・升・序数と byte_at_cell）が、行頭から column_advance と
//      grid_placed_cell だけで素朴に歩いた値と一致する（各行の各コードポイントの先頭と終わり）。
//   2. GridCells の cursor_at・point_of・byte_of・rows が同じ素朴な歩きと一致し、幅のある文字では
//      point_of と byte_of が往復する。core の grid_cell_of も同じ点を答える（CRLF の文書の終端の
//      '\r' の 1 点を除く・terminal_carriage_return の注釈）。
//   3. 1 回の問い合わせの GridCells::scanned() は、その行に掛かる plain でない塊の数を超えない。
//   4. ランダムな replaced（挿入・削除・置換・改行と塊の境目をまたぐ・大きな挿入と削除）の後、
//      replaced で写した索引の塊が本文を覆い、各塊の始まりの累積が素朴な歩きと一致し、1〜3 の答えも
//      同じ。一から作った for_grid と境目が同じなら要約の全部の欄も同じ。境目の違いは切り直しと
//      合わせの規則（VirtualColumnIndex.hpp の注釈）で起きるので数えるだけにする。
// 素朴な歩きは製品の GridCells と VirtualColumnIndex を呼ばない。行の端（行頭・内容の終わり・
// '\n'）は TextBuffer の line_start / line_end から読む（行の規則は --text-buffer-property が
// 守る）。長い行と大きな本文では見る行と位置を固定 seed で間引く（短い行は全部を見る）。回数は
// Debug（ASan）で --text-buffer-property と同じ程度に終わる値で、50 倍の走りは property_scale を
// 手で 50 にして回す（commit しない）。生成器は std::mt19937_64 の剰余だけを使い、分布の型は
// 使わない。
#include "Grid.hpp"
#include "GridCells.hpp"
#include "GridPoint.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "VirtualColumn.hpp"
#include "VirtualColumnChunk.hpp"
#include "VirtualColumnIndex.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::column_advance;
using core::grid_cell_of;
using core::grid_placed_cell;
using core::GridPoint;
using core::LineNumber;
using core::Offset;
using core::TextBuffer;
using core::VirtualColumnChunk;
using core::VirtualColumnIndex;

using Random = std::mt19937_64;
// 素朴な歩きの 1 文字（最後の 1 つは '\n' の手前の終わり）: バイト位置・コードポイント・
// 手前までの桁・手前までの升・始まる升・伸ばす桁（升）・行頭からの序数。
using Placed = std::array<std::size_t, 7>;
// 1 行の端: 番号（1 始まり）・行頭・内容の終わり・'\n' の位置（最終行は本文の末尾）。
using LineSpan = std::array<std::size_t, 4>;
// 見る 1 行: 端と、行頭から '\n' の手前までの素朴な歩き。
using LineModel = std::pair<LineSpan, std::vector<Placed>>;
// 1 回の編集: 始まり・終わり・入れる本文。
using PlannedEdit = std::tuple<std::size_t, std::size_t, std::string>;
// case の形: 幅・編集の段の数・大きな挿入と削除のバイト数の上限（0 は大きな編集をしない）。
using CaseShape = std::array<std::size_t, 3>;

constexpr std::size_t placed_offset = 0;
constexpr std::size_t placed_value = 1;
constexpr std::size_t placed_column = 2;
constexpr std::size_t placed_cell = 3;
constexpr std::size_t placed_start = 4;
constexpr std::size_t placed_advance = 5;
constexpr std::size_t placed_ordinal = 6;
constexpr std::size_t span_number = 0;
constexpr std::size_t span_begin = 1;
constexpr std::size_t span_end = 2;
constexpr std::size_t span_stop = 3;
constexpr std::size_t shape_columns = 0;
constexpr std::size_t shape_steps = 1;
constexpr std::size_t shape_big = 2;

constexpr char32_t tab_character = U'\t';
constexpr std::uint64_t property_seed = 0x6772'6964'3933'36ULL;
// 50 倍の走りではここを 50 にする（commit しない）。
constexpr std::size_t property_scale = 1;
constexpr std::size_t small_cases = 16 * property_scale;
constexpr std::size_t long_cases = 2 * property_scale;
constexpr std::size_t crossing_cases = 1 * property_scale;
// 1 回の照合で見る行の数と、1 行で見る位置・升の数の上限（これ以下なら全部を見る）。
constexpr std::size_t line_limit = 24;
// 長い行では 1 回の問い合わせが行の長さぶん歩くので、見る数を point_budget / 行の長さまで減らす
// （両端の 1 つずつと乱数で最低 4）。
constexpr std::size_t point_limit = 16;
constexpr std::size_t point_budget = 1'000;
constexpr std::size_t fewest_points = 4;
constexpr std::size_t longest_line = 20'000;
constexpr std::size_t chunk_bytes = 64U * 1024U;
// 端の本文を通す幅。
constexpr std::array<std::size_t, 3> edge_columns{{1, 3, 7}};

// 本文の 1 コードポイント（表示できる ASCII は別に選ぶ）。Tab・全角・絵文字・結合文字 U+0301・
// 書式用文字 U+200B・C1 の U+0085・制御文字・DEL・孤立した CR・2 バイトの é。
constexpr std::array<std::string_view, 10> special_fragments{{
    "\t",
    "\xe6\x97\xa5",
    "\xf0\x9f\x98\x80",
    "\xcc\x81",
    "\xe2\x80\x8b",
    "\xc2\x85",
    "\x01",
    "\x7f",
    "\r",
    "\xc3\xa9",
}};
constexpr std::string_view plain_letters = "abcxyzQ ~0";

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return bound == 0 ? 0 : static_cast<std::size_t>(random() % bound);
}

// count コードポイントの 1 行。半分は表示できる ASCII、残りは特別な断片。
[[nodiscard]] std::string generated_line(Random &random, std::size_t count)
{
    std::string line;
    for (std::size_t index = 0; index < count; ++index)
    {
        if (below(random, 2) == 0)
        {
            line += plain_letters.at(below(random, plain_letters.size()));
            continue;
        }
        line += special_fragments.at(below(random, special_fragments.size()));
    }
    return line;
}

// 短い行が多く、ときどき数百コードポイントの行を混ぜる。
[[nodiscard]] std::size_t short_length(Random &random)
{
    const auto kind = below(random, 10);
    if (kind < 6)
    {
        return below(random, 12);
    }
    return kind < 9 ? below(random, 100) : below(random, 400);
}

[[nodiscard]] std::string_view separator_of(Random &random)
{
    return below(random, 2) == 0 ? std::string_view{"\n"} : std::string_view{"\r\n"};
}

[[nodiscard]] std::string generated_text(Random &random, std::size_t lines, std::size_t long_at)
{
    const auto separator = separator_of(random);
    std::string text;
    for (std::size_t index = 0; index < lines; ++index)
    {
        text += index == 0 ? std::string_view{} : separator;
        const auto count =
            index == long_at ? 1 + below(random, longest_line) : short_length(random);
        text += generated_line(random, count);
    }
    return text;
}

// 幅は半分が 1〜12（送りが頻繁に起きる）、半分が 1〜240。
[[nodiscard]] std::size_t columns_of(Random &random)
{
    return below(random, 2) == 0 ? 1 + below(random, 12) : 1 + below(random, 240);
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

[[nodiscard]] bool continues(char value)
{
    return (static_cast<unsigned char>(value) & 0xC0U) == 0x80U;
}

// at から後ろへ進めた最初の code point の先頭（本文の末尾を越えない）。
[[nodiscard]] std::size_t boundary_from(std::string_view text, std::size_t at)
{
    at = std::min(at, text.size());
    while (at < text.size() && continues(text[at]))
    {
        ++at;
    }
    return at;
}

// 素朴な歩き。行頭から 1 文字ずつ column_advance で桁を足し、Tab 以外は grid_placed_cell で
// 置く升を決める（columns が 0 なら送らない）。最後に終わりの累積を足す。
// state の位置の 1 文字を置き（値・伸ばす桁・始まる升を書く）、置いた値を返して state
// を次へ進める。
[[nodiscard]] Placed placed_next(std::string_view model, Placed &state, std::size_t columns)
{
    const auto [value, length] = decoded_at(model, state.at(placed_offset));
    const auto cell = state.at(placed_cell);
    const auto advance = column_advance(value, state.at(placed_column));
    state.at(placed_value) = value;
    state.at(placed_advance) = advance;
    state.at(placed_start) =
        value == tab_character ? cell : grid_placed_cell(cell, advance, columns);
    const auto placed = state;
    state.at(placed_offset) += length;
    state.at(placed_column) += advance;
    state.at(placed_cell) = state.at(placed_start) + advance;
    ++state.at(placed_ordinal);
    return placed;
}

[[nodiscard]] std::vector<Placed> walked_line(std::string_view model, std::size_t begin,
                                              std::size_t stop, std::size_t columns)
{
    std::vector<Placed> line;
    Placed state{begin, 0, 0, 0, 0, 0, 0};
    while (state.at(placed_offset) < stop)
    {
        line.push_back(placed_next(model, state, columns));
    }
    state.at(placed_value) = 0;
    state.at(placed_advance) = 0;
    state.at(placed_start) = state.at(placed_cell);
    line.push_back(state);
    return line;
}

[[nodiscard]] LineSpan span_of(const TextBuffer &text, std::size_t number)
{
    const LineNumber line{number};
    const auto stop =
        number < text.line_count() ? text.line_terminator_end(line).value - 1 : text.size_bytes();
    return {number, text.line_start(line).value, text.line_end(line).value, stop};
}

[[nodiscard]] LineModel model_of(const TextBuffer &text, std::string_view model, std::size_t number,
                                 std::size_t columns)
{
    const auto span = span_of(text, number);
    return {span, walked_line(model, span.at(span_begin), span.at(span_stop), columns)};
}

// 0〜count-1 の中から見る位置。count が上限以下なら全部、そうでなければ両端と乱数。
[[nodiscard]] std::vector<std::size_t> picks_of(Random &random, std::size_t count,
                                                std::size_t limit)
{
    std::vector<std::size_t> picks;
    if (count <= limit)
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            picks.push_back(index);
        }
        return picks;
    }
    picks.push_back(0);
    picks.push_back(count - 1);
    while (picks.size() < limit)
    {
        picks.push_back(below(random, count));
    }
    return picks;
}

// 内容の終わりにある歩きの位置（CRLF の行では '\r' の手前）。
[[nodiscard]] std::size_t content_count(const LineModel &line)
{
    std::size_t count = 0;
    while (line.second.at(count).at(placed_offset) < line.first.at(span_end))
    {
        ++count;
    }
    return count;
}

[[nodiscard]] std::size_t limit_of(const LineModel &line)
{
    return std::clamp(point_budget / line.second.size(), fewest_points, point_limit);
}

[[nodiscard]] GridPoint point_at(std::size_t cell, std::size_t columns)
{
    return GridPoint{cell / columns, cell % columns};
}

// 素朴な点。幅 0 の文字は直前の文字の点（行頭なら手前の升）、ほかは始まる升。内容の終わりが
// 画面行の右端にちょうど達したときは、その画面行の右端。
[[nodiscard]] std::vector<GridPoint> points_of(const LineModel &line, std::size_t columns)
{
    const auto count = content_count(line);
    std::vector<GridPoint> points;
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto &placed = line.second.at(index);
        const bool zero =
            placed.at(placed_advance) == 0 && placed.at(placed_value) != tab_character;
        const auto own = point_at(placed.at(placed_start), columns);
        points.push_back(
            zero ? (index == 0 ? point_at(placed.at(placed_cell), columns) : points.at(index - 1))
                 : own);
    }
    const auto cell = line.second.at(count).at(placed_cell);
    points.push_back(cell > 0 && cell % columns == 0 ? GridPoint{(cell / columns) - 1, columns}
                                                     : point_at(cell, columns));
    return points;
}

// 素朴な逆引き。升 target
// を占める（送りの空きなら送られた）最初の文字の先頭。無ければ内容の終わり。
[[nodiscard]] std::size_t byte_of_cell(const LineModel &line, std::size_t target)
{
    const auto count = content_count(line);
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto &placed = line.second.at(index);
        if (placed.at(placed_advance) > 0 &&
            target < placed.at(placed_start) + placed.at(placed_advance))
        {
            return placed.at(placed_offset);
        }
    }
    return line.first.at(span_end);
}

[[nodiscard]] const VirtualColumnIndex &held_of(const TextBuffer &text)
{
    return text.virtual_columns().value().get();
}

// 1. for_grid と本文が持つ索引の cursor_at が素朴な累積と一致する（'\n' の手前の終わりまで）。
[[nodiscard]] bool index_cursors_match(const TextBuffer &text, const VirtualColumnIndex &grid,
                                       const LineModel &line, Random &random)
{
    bool same = true;
    for (const auto pick : picks_of(random, line.second.size(), limit_of(line)))
    {
        const auto &placed = line.second.at(pick);
        const Offset at{placed.at(placed_offset)};
        const auto cursor = grid.cursor_at(text, at);
        const auto held = held_of(text).cursor_at(text, at);
        same = same && cursor.column == placed.at(placed_column) &&
               cursor.cell == placed.at(placed_cell) &&
               cursor.code_point == placed.at(placed_ordinal) && cursor.scanned <= 1 &&
               held.column == placed.at(placed_column) && held.cell == held.column &&
               held.code_point == placed.at(placed_ordinal);
    }
    return same;
}

// CRLF の文書で '\n' の続かない最終行の末尾の '\r' は、TextBuffer が内容の終わりの外に置く
// （--text-buffer-property の模型と同じ規則）。GridCells は line_end に従ってそこを行の終わりとして
// 答えるが、core の grid_cell_of は '\n' が続くときだけ '\r' を終わりと見て、文字 `^M` として置く
// （#936 の報告・最小の入力は verify_terminal_carriage_return）。直しは別 Issue なので、その 1
// 点だけ grid_cell_of の照合から外す。
[[nodiscard]] bool terminal_carriage_return(const TextBuffer &text, const LineSpan &span,
                                            std::size_t at)
{
    return at == span.at(span_end) && span.at(span_end) < span.at(span_stop) &&
           span.at(span_stop) == text.size_bytes();
}

// 2. GridCells の累積・点・往復と core の grid_cell_of が素朴な歩きと一致する（内容の終わりまで）。
[[nodiscard]] bool grid_points_match(const TextBuffer &text, const VirtualColumnIndex &grid,
                                     const LineModel &line, Random &random)
{
    const auto columns = grid.columns();
    application::GridCells cells(held_of(text), text, columns);
    const LineNumber number{line.first.at(span_number)};
    const auto points = points_of(line, columns);
    bool same = true;
    for (const auto pick : picks_of(random, points.size(), limit_of(line)))
    {
        const auto &placed = line.second.at(pick);
        const Offset at{placed.at(placed_offset)};
        const auto cursor = cells.cursor_at(number, at);
        const auto point = cells.point_of(number, at);
        const bool wide = placed.at(placed_advance) > 0 && pick + 1 < points.size();
        same = same && cursor.column == placed.at(placed_column) &&
               cursor.cell == placed.at(placed_cell) &&
               cursor.code_point == placed.at(placed_ordinal) && point == points.at(pick) &&
               (terminal_carriage_return(text, line.first, at.value) ||
                grid_cell_of(grid, text, at) == points.at(pick)) &&
               (!wide || cells.byte_of(number, point) == at);
    }
    return same;
}

// 2. rows は素朴な升の合計を幅で切り上げた数（空行も 1 行）。
[[nodiscard]] bool rows_match(const TextBuffer &text, const VirtualColumnIndex &grid,
                              const LineModel &line)
{
    const auto columns = grid.columns();
    application::GridCells cells(held_of(text), text, columns);
    const auto total = line.second.at(content_count(line)).at(placed_cell);
    const auto expected = std::max<std::size_t>(1, (total + columns - 1) / columns);
    return cells.rows(LineNumber{line.first.at(span_number)}) == expected;
}

// 1・2. 升 → バイトの逆引き（行の外の升を含む）が素朴な逆引きと一致する。
[[nodiscard]] bool cell_bytes_match(const TextBuffer &text, const VirtualColumnIndex &grid,
                                    const LineModel &line, Random &random)
{
    const auto columns = grid.columns();
    application::GridCells cells(held_of(text), text, columns);
    const LineNumber number{line.first.at(span_number)};
    const auto total = line.second.at(content_count(line)).at(placed_cell);
    bool same = true;
    for (const auto target : picks_of(random, total + columns + 2, limit_of(line)))
    {
        const auto expected = byte_of_cell(line, target);
        same = same && grid.byte_at_cell(text, number, target).value == expected &&
               cells.byte_of(number, point_at(target, columns)).value == expected;
    }
    return same;
}

// 3. 行の [begin, end) に掛かる plain でない塊の数。
[[nodiscard]] std::size_t walked_chunks(const VirtualColumnIndex &index, const LineSpan &span)
{
    std::size_t count = 0;
    for (const auto &chunk : index.chunks())
    {
        const bool overlaps =
            chunk.start < span.at(span_end) && chunk.start + chunk.bytes > span.at(span_begin);
        count += overlaps && !chunk.plain ? 1 : 0;
    }
    return count;
}

// 3. 1 回の問い合わせ（行の終わりの累積・行の外の升の逆引き）で歩いた部分は上限を超えない。
[[nodiscard]] bool scanned_bounded(const TextBuffer &text, const VirtualColumnIndex &grid,
                                   const LineModel &line)
{
    const LineNumber number{line.first.at(span_number)};
    const auto bound = walked_chunks(held_of(text), line.first);
    application::GridCells ended(held_of(text), text, grid.columns());
    static_cast<void>(ended.cursor_at(number, Offset{line.first.at(span_end)}));
    application::GridCells beyond(held_of(text), text, grid.columns());
    static_cast<void>(beyond.byte_of(number, GridPoint{line.second.size() + 1, 0}));
    return ended.scanned() <= bound && beyond.scanned() <= bound;
}

void verify_line(const TextBuffer &text, const VirtualColumnIndex &grid, const LineModel &line,
                 Random &random)
{
    expect(index_cursors_match(text, grid, line, random),
           "for_grid and the held index answer the naive column, cell and ordinal sums");
    expect(grid_points_match(text, grid, line, random),
           "GridCells and grid_cell_of answer the naive points and round trip");
    expect(rows_match(text, grid, line), "GridCells::rows is the naive cell total by the columns");
    expect(cell_bytes_match(text, grid, line, random),
           "byte_at_cell and GridCells::byte_of land where the naive walk lands");
    expect(scanned_bounded(text, grid, line),
           "one GridCells query walks no more parts than the non-plain chunks of the line");
}

[[nodiscard]] bool start_matches(const VirtualColumnChunk &chunk, const Placed &state)
{
    return chunk.column == state.at(placed_column) && chunk.cell == state.at(placed_cell) &&
           chunk.code_point == state.at(placed_ordinal);
}

// 4. 各塊の始まりが code point の先頭で、その累積が本文を頭から素朴に歩いた値と一致する（'\n' の
// 後ろで行頭に戻る）。columns が 0 の索引（本文が持つ索引）は送らないので、升は桁と同じ。
[[nodiscard]] bool chunk_starts_match(std::string_view model, const VirtualColumnIndex &index)
{
    const auto chunks = index.chunks();
    std::size_t next = 0;
    bool same = true;
    Placed state{0, 0, 0, 0, 0, 0, 0};
    while (next < chunks.size())
    {
        const auto at = state.at(placed_offset);
        if (chunks[next].start == at)
        {
            same = same && start_matches(chunks[next], state);
            ++next;
            continue;
        }
        if (chunks[next].start < at || at >= model.size())
        {
            return false;
        }
        const bool newline = model[at] == '\n';
        static_cast<void>(placed_next(model, state, index.columns()));
        state = newline ? Placed{at + 1, 0, 0, 0, 0, 0, 0} : state;
    }
    return same;
}

// 4. 塊の並びが本文を隙間なく覆い、前の '\n' の数・中の '\n' の数・plain が本文と一致する。
// 大きさは 0 より大きく、切り直しの上限（128 KiB と code point の余り）以下。
[[nodiscard]] bool chunk_layout_matches(std::string_view model, const VirtualColumnIndex &index)
{
    constexpr std::size_t longest = (2 * chunk_bytes) + 3;
    std::size_t at = 0;
    std::size_t line = 0;
    for (const auto &chunk : index.chunks())
    {
        if (chunk.start != at || chunk.bytes == 0 || chunk.bytes > longest ||
            at + chunk.bytes > model.size())
        {
            return false;
        }
        const auto bytes = model.substr(chunk.start, chunk.bytes);
        const auto breaks = static_cast<std::size_t>(std::ranges::count(bytes, '\n'));
        const bool plain = std::ranges::all_of(
            bytes, [](char byte) { return byte == '\n' || (byte >= 0x20 && byte <= 0x7E); });
        if (chunk.line != line || chunk.line_breaks != breaks || chunk.plain != plain)
        {
            return false;
        }
        at += chunk.bytes;
        line += breaks;
    }
    return at == model.size();
}

[[nodiscard]] bool same_counts(const VirtualColumnChunk &left, const VirtualColumnChunk &right)
{
    return left.start == right.start && left.bytes == right.bytes && left.column == right.column &&
           left.cell == right.cell && left.code_point == right.code_point &&
           left.lead == right.lead && left.rest == right.rest && left.padding == right.padding &&
           left.tail_cell == right.tail_cell && left.code_points == right.code_points &&
           left.line == right.line && left.line_breaks == right.line_breaks;
}

[[nodiscard]] bool same_summary(const VirtualColumnChunk &left, const VirtualColumnChunk &right)
{
    return same_counts(left, right) && left.plain == right.plain && left.tab == right.tab &&
           left.resets == right.resets && left.wide == right.wide;
}

// 4. replaced で写した索引と一から作った索引の境目が同じか（違いは切り直しと合わせの規則で
// あり得る）と、境目が同じなら要約の全部の欄が同じか。
[[nodiscard]] std::pair<bool, bool> compared(const VirtualColumnIndex &edited,
                                             const VirtualColumnIndex &fresh)
{
    const auto left = edited.chunks();
    const auto right = fresh.chunks();
    bool cuts = left.size() == right.size();
    for (std::size_t index = 0; cuts && index < left.size(); ++index)
    {
        cuts = left[index].start == right[index].start && left[index].bytes == right[index].bytes;
    }
    bool summaries = edited.columns() == fresh.columns();
    for (std::size_t index = 0; cuts && index < left.size(); ++index)
    {
        summaries = summaries && same_summary(left[index], right[index]);
    }
    return {cuts, summaries};
}

void verify_text(const TextBuffer &text, const VirtualColumnIndex &grid, std::string_view model,
                 Random &random)
{
    for (const auto pick : picks_of(random, text.line_count(), line_limit))
    {
        verify_line(text, grid, model_of(text, model, pick + 1, grid.columns()), random);
    }
    expect(chunk_layout_matches(model, grid) && chunk_starts_match(model, grid),
           "the chunks of the grid index cover the text and start at the naive sums");
    expect(chunk_layout_matches(model, held_of(text)) && chunk_starts_match(model, held_of(text)),
           "the chunks of the held index cover the text and start at the naive sums");
}

// 入れる本文。1 コードポイントずつ選び、8 回に 1 回は LF か CRLF を入れる。
[[nodiscard]] std::string inserted_of(Random &random, std::size_t count)
{
    std::string inserted;
    for (std::size_t index = 0; index < count; ++index)
    {
        inserted +=
            below(random, 8) == 0 ? std::string{separator_of(random)} : generated_line(random, 1);
    }
    return inserted;
}

// 塊の境目の少し手前（塊が 1 つなら乱数の位置）。
[[nodiscard]] std::size_t near_cut(Random &random, std::string_view model,
                                   const VirtualColumnIndex &index)
{
    if (index.chunk_count() < 2)
    {
        return boundary_from(model, below(random, model.size() + 1));
    }
    const auto start = index.chunks()[1 + below(random, index.chunk_count() - 1)].start;
    return boundary_from(model, start - std::min(start, below(random, 8)));
}

// 1 回の編集。道 0〜2 は挿入、3〜4 は削除、5〜6 は置換、7 は塊の境目をまたぐ置換。big が 0 で
// なければ 8〜11 で big バイトまでの大きな挿入・削除・置換と、塊の境目からの大きな削除をする。
[[nodiscard]] PlannedEdit edit_of(Random &random, std::string_view model,
                                  const VirtualColumnIndex &index, std::size_t big)
{
    // 道ごとの形: 塊の境目から始めるか・挿入するか（削除だけの道は 3・4・9・11）。
    constexpr std::array<bool, 12> crossing_paths{
        {false, false, false, false, false, false, false, true, false, false, false, true}};
    constexpr std::array<bool, 12> inserting_paths{
        {true, true, true, false, false, true, true, true, true, false, true, false}};
    const auto path = below(random, big == 0 ? 8 : 12);
    const bool crossing = crossing_paths.at(path);
    const bool large = path >= 8;
    const auto begin = crossing ? near_cut(random, model, index)
                                : boundary_from(model, below(random, model.size() + 1));
    const auto reach = large ? below(random, big + 1) : below(random, crossing ? 200 : 48);
    const auto end = path <= 2 ? begin : boundary_from(model, begin + reach);
    const auto count = large ? below(random, (big / 2) + 1) : below(random, 16);
    return {begin, end, inserting_paths.at(path) ? inserted_of(random, count) : std::string{}};
}

// 1 つの case。本文が持つ索引（GridCells が借りる）と for_grid を同じ編集の列で動かし、各段で
// 照合する。一から作った索引と境目が違った段の数を返す。
std::size_t run_case(Random &random, const std::string &initial, CaseShape shape)
{
    const auto columns = shape.at(shape_columns);
    auto text = buffer_of(initial).with_virtual_columns();
    auto grid = VirtualColumnIndex::for_grid(text, columns);
    std::string model = initial;
    verify_text(text, grid, model, random);
    std::size_t differing = 0;
    for (std::size_t step = 0; step < shape.at(shape_steps); ++step)
    {
        const auto [begin, end, inserted] = edit_of(random, model, grid, shape.at(shape_big));
        text = text.replaced(Offset{begin}, Offset{end}, inserted);
        grid = grid.replaced(text, Offset{begin}, Offset{end}, inserted);
        model.replace(begin, end - begin, inserted);
        const auto [cuts, summaries] = compared(grid, VirtualColumnIndex::for_grid(text, columns));
        expect(summaries, "a replaced grid index with the fresh cuts has the fresh summaries");
        differing += cuts ? 0 : 1;
        verify_text(text, grid, model, random);
    }
    return differing;
}

// 手で選んだ端の本文（CRLF の終端の '\r'・行頭の結合文字・右端の全角と Tab・空）。
void verify_edge_texts(Random &random)
{
    constexpr std::array<std::string_view, 10> texts{{
        "",
        "\r",
        "a\r\nb\r",
        "\r\n\r\n\r\r",
        "a\nb\r",
        "\xcc\x81\xcc\x81x\n\xcc\x81",
        "abc\xe6\x97\xa5\xe6\x97\xa5\t\xe6\x97\xa5",
        "\xe2\x80\x8b\xe2\x80\x8b\xc2\x85\x01\x7f\t",
        "abcd\nabcd\r\n",
        "\t\xe6\x97\xa5"
        "e\xcc\x81\r\n\xe2\x80\x8b\t",
    }};
    for (const auto text : texts)
    {
        for (const auto columns : edge_columns)
        {
            static_cast<void>(run_case(random, std::string{text}, CaseShape{columns, 2, 0}));
        }
    }
}

void verify_small_cases(Random &random)
{
    for (std::size_t index = 0; index < small_cases; ++index)
    {
        const auto text = generated_text(random, 1 + below(random, 8), std::string::npos);
        static_cast<void>(run_case(random, text, CaseShape{columns_of(random), 4, 0}));
    }
}

// 1 行が 1〜20,000 コードポイントの行を 1 つ含む本文。
void verify_long_cases(Random &random)
{
    for (std::size_t index = 0; index < long_cases; ++index)
    {
        const auto text = generated_text(random, 3, 1);
        static_cast<void>(run_case(random, text, CaseShape{columns_of(random), 1, 0}));
    }
}

// 64 KiB の境目を 2 つ以上またぐ本文: plain の ASCII の行・混ぜた 20,000 コードポイントの行・
// 30,000 の ASCII の行・25,000 の全角だけの行（送りの余りが後ろの塊へ運ばれる）・ASCII の行・
// 混ぜた尻尾。大きな挿入（128 KiB を越える範囲の切り直し）と削除（16 KiB を下回る範囲の合わせ）も
// 施す。
[[nodiscard]] std::string crossing_text(Random &random)
{
    std::string text;
    while (text.size() < 50U * 1024U)
    {
        text += std::string(59, static_cast<char>('a' + (text.size() % 26)));
        text += '\n';
    }
    text += generated_line(random, longest_line) + "\n" + std::string(30'000, 'x') + "\n";
    for (std::size_t count = 0; count < 25'000; ++count)
    {
        text += "\xe6\x97\xa5";
    }
    text += '\n';
    while (text.size() < 200U * 1024U)
    {
        text += std::string(59, 'q');
        text += '\n';
    }
    return text + generated_line(random, 2'000);
}

void verify_crossing_cases(Random &random)
{
    for (std::size_t index = 0; index < crossing_cases; ++index)
    {
        const auto text = crossing_text(random);
        expect(VirtualColumnIndex::from_text(buffer_of(text)).chunk_count() >= 4,
               "the crossing text spans several chunks");
        static_cast<void>(run_case(random, text, CaseShape{columns_of(random), 2, 72U * 1024U}));
    }
}

// #936 の走りで見つけた差の最小の入力（回帰 case）。CRLF の文書 "\r\nb\r" の 2 行目は "b" で、
// '\n' の続かない末尾の '\r' は内容の外。幅 1 では "b" が升 0 を埋めて画面行の右端にちょうど
// 達するので、GridCells は行の終わりを {0, 1} と答える（core の grid_cell_of は '\r' を `^M` として
// 置いて {1, 0} を答える。terminal_carriage_return の注釈）。
void verify_terminal_carriage_return()
{
    const auto text = buffer_of("\r\nb\r").with_virtual_columns();
    application::GridCells cells(held_of(text), text, 1);
    const LineNumber line{2};
    expect(text.line_end(line).value == 3,
           "the terminal CR of a CRLF text without a following LF is outside the content");
    expect(cells.point_of(line, Offset{3}) == GridPoint{0, 1} && cells.rows(line) == 1 &&
               cells.byte_of(line, GridPoint{0, 1}).value == 3,
           "GridCells puts the end before a terminal CR on the right edge of its row");
}
} // namespace

void verify_grid_cells_property_scope()
{
    verify_terminal_carriage_return();
    Random random{property_seed};
    verify_edge_texts(random);
    verify_small_cases(random);
    verify_long_cases(random);
    verify_crossing_cases(random);
}
} // namespace nenenib::tests
