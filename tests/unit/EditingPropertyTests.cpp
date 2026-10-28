// scope `--editing-property` の単体テスト（#955・ADR 0009 / 0014 / 0052 / 0053 / 0055 / 0065・
// ADR 0042 決定 2）。固定 seed の決定的な本文（0〜12 行・LF か CRLF）と通常モードの編集の意図の列
// （20〜80 歩）を controller に流し、各歩の後に次を見る。
//   1. 本文は正しい UTF-8 で、CRLF の文書に孤立した LF が無い。単独の CR は貼り付けが文字のまま
//      残す（ADR 0055 の決定 3）ので、単独の CR を含む本文を貼っていない走りに限り、CRLF の文書に
//      単独の CR が無く、LF の文書に CR が無い。文書の改行の形は開いた後に変わらない。
//   2. キャレットと選択の錨は本文の内で、行の内容の内（CRLF の対の間に無い）にあり、幅 0 の
//      コードポイント（結合文字・VS）の前に無い（ADR 0053 の 1 文字の境目）。
//   3. 編集の意図の効果を直前に読んだ状態から模型の std::string の操作で予測して比べる。
//   4. 途中の undo → redo は本文を変えず、編集の直後ならキャレットも変えない（通常モードの redo は
//      戻した本文の末尾に置く・ADR 0052 の決定 6）。列の終わりに redo で端まで進めてから undo を
//      歩の数より多く打つと開いた本文に戻り、同じ数の redo で undo の前の本文に戻る。
//   5. 同じ組を 2 回流すと同じ最後の本文と位置。
// controller はオフセットを読む口を持たないので、キャレットと錨は frame の行の桁（コードポイント
// の序数 + 1）と選択の区間から、模型の行の切り方で本文の位置へ戻す。模型の行は TextBuffer の規則
// の写し: '\n' で切り、CRLF の文書だけ内容の終わりの '\r' を 1 つ外す（最終行の末尾の単独の '\r'
// も外す）。
// 削除の 1 文字は通常モードの規則（ADR 0065 の決定 4・5）の写し: Backspace はコードポイント 1 つ
// （CRLF の文書の \r\n は対）で、消した先頭が VS ならその前の 1 つまで。
// Delete はコードポイント 1 つ（同じく対）と、続く幅 0 のコードポイントと、さらに続く VS。
// 幅 0 の判定だけは表の正本 display_width を引く。
#include "CaretAffinity.hpp"
#include "CaretMotion.hpp"
#include "ClauseEmphasis.hpp"
#include "ClipboardAction.hpp"
#include "ClipboardOperation.hpp"
#include "Column.hpp"
#include "Composition.hpp"
#include "CompositionClause.hpp"
#include "DeleteDirection.hpp"
#include "DisplayWidth.hpp"
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "HistoryDirection.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "Scopes.hpp"
#include "SelectionAnchoring.hpp"
#include "SelectionPresence.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <iterator>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

using Random = std::mt19937_64;
// 替え玉のクリップボードに置く本文（nullopt は「空」の失敗）。
using Hold = std::optional<std::string>;
// IME の 1 語: 変換中の文字列と、確定する本文（nullopt は取消）。
using Word = std::pair<core::Composition, std::optional<std::string>>;
// 1 歩の動作。std::monostate は途中の undo → redo の往復。
using Action = std::variant<app::EditorIntent, Hold, Word, std::monostate>;
// 1 歩: 報告に出す名前と動作。
using Step = std::pair<std::string, Action>;
// 1 組: 開く本文と歩の列。
using Case = std::pair<std::string, std::vector<Step>>;
// 選択の錨とキャレット（本文のバイトの位置）。
using Positions = std::pair<std::size_t, std::size_t>;
// 読んだ状態: 本文と位置。
using Seen = std::pair<std::string, Positions>;
// 予測: 本文と、予測できるなら位置。
using Expected = std::pair<std::string, std::optional<Positions>>;
// 模型の 1 行: 始まり・内容の終わり・次の行の始まり。
using Line = std::tuple<std::size_t, std::size_t, std::size_t>;
using Lines = std::vector<Line>;
// 走りの状態: 直前に読んだ状態・文書が CRLF か・単独の CR を含む本文を貼ったか・直前の歩が
// 本文を変えた編集か。
using Run = std::tuple<Seen, bool, bool, bool>;
using Reason = std::string_view;
// 組の結果: 最初に破れた不変条件（空なら無し）と最後の状態。
using Outcome = std::pair<Reason, Seen>;
using Check = Reason (*)(const Case &);

// Debug で 1 秒以内に収まる回数（#955）。50 倍の走りは rounds を 50 にして手元で回し、commit
// しない。
constexpr std::size_t rounds = 1U;
constexpr std::size_t family_cases = 20U * rounds;
constexpr std::size_t line_limit = 13U;
constexpr std::size_t step_floor = 20U;
constexpr std::size_t step_spread = 61U;
constexpr std::size_t visible_lines = 400U;
constexpr std::size_t point_bytes = 4U;

// 本文の語。ASCII の語・空白・Tab・日本語（かな・漢字・全角英数・半角カナ）・結合文字つきの
// 1 文字（e + 鋭アクセント・か + 結合濁点・❤ + VS16・葛 + VS17）。
constexpr std::array<std::string_view, 14> body_tokens{
    "ab",   "Hello",  "x",  " ",       "  ",       "\t",           "日本語",
    "かな", "ＡＢ１", "ｶﾅ", "e\u0301", "か\u3099", "\u2764\uFE0F", "葛\U000E0100"};
// InsertText の文字（改行を含めない）。
constexpr std::array<std::string_view, 10> typed_characters{"a",  "Z",  "0",  " ",  "\t",
                                                            "あ", "漢", "Ａ", "ー", "。"};
// 変換中の文字。
constexpr std::array<std::string_view, 6> kana{"に", "ほ", "ん", "ご", "変", "換"};
// 替え玉のクリップボードの本文（LF・CRLF・単独の CR・空・日本語）。
constexpr std::array<std::string_view, 9> clipboard_texts{
    "", "x\r\ny", "a\nb\n", "\r", "改\r行", "日本語", "p\r\n\r\nq", "z\r", "\r\n"};
constexpr std::array<core::CaretMotion, 12> motions{core::CaretMotion::previous_character,
                                                    core::CaretMotion::next_character,
                                                    core::CaretMotion::previous_word,
                                                    core::CaretMotion::next_word,
                                                    core::CaretMotion::previous_line,
                                                    core::CaretMotion::next_line,
                                                    core::CaretMotion::line_start,
                                                    core::CaretMotion::line_end,
                                                    core::CaretMotion::page_up,
                                                    core::CaretMotion::page_down,
                                                    core::CaretMotion::document_start,
                                                    core::CaretMotion::document_end};
constexpr std::array<core::SelectionAnchoring, 2> anchorings{core::SelectionAnchoring::collapse,
                                                             core::SelectionAnchoring::extend};
constexpr std::array<app::ClipboardOperation, 4> clipboard_operations{
    app::ClipboardOperation::copy, app::ClipboardOperation::cut, app::ClipboardOperation::paste,
    app::ClipboardOperation::paste_plain};
constexpr std::array<std::string_view, 4> clipboard_names{"copy", "cut", "paste", "paste_plain"};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

template <typename R> [[nodiscard]] auto &seen_of(R &run)
{
    return std::get<0>(run);
}
template <typename R> [[nodiscard]] auto &crlf_of(R &run)
{
    return std::get<1>(run);
}
template <typename R> [[nodiscard]] auto &tainted_of(R &run)
{
    return std::get<2>(run);
}
template <typename R> [[nodiscard]] auto &edited_of(R &run)
{
    return std::get<3>(run);
}

// ---------------------------------------------------------------- 素朴な UTF-8 と行

[[nodiscard]] bool lead_byte(char unit) noexcept
{
    return (static_cast<unsigned char>(unit) & 0xC0U) != 0x80U;
}

[[nodiscard]] std::size_t point_after(std::string_view text, std::size_t at)
{
    std::size_t next = std::min(at + 1U, text.size());
    while (next < text.size() && !lead_byte(text[next]))
    {
        ++next;
    }
    return next;
}

[[nodiscard]] std::size_t point_before(std::string_view text, std::size_t at)
{
    std::size_t previous = at == 0U ? 0U : at - 1U;
    while (previous > 0U && !lead_byte(text[previous]))
    {
        --previous;
    }
    return previous;
}

// at のコードポイント（本文の外は 0）。正しい UTF-8 だけを読む。
[[nodiscard]] char32_t point_at(std::string_view text, std::size_t at)
{
    if (at >= text.size())
    {
        return 0;
    }
    const std::size_t end = point_after(text, at);
    const auto first = static_cast<unsigned char>(text[at]);
    constexpr std::array<unsigned, 5> lead_masks{0x00U, 0x7FU, 0x1FU, 0x0FU, 0x07U};
    char32_t value = first & lead_masks.at(std::min(end - at, point_bytes));
    for (std::size_t index = at + 1U; index < end; ++index)
    {
        value = (value << 6U) | (static_cast<unsigned char>(text[index]) & 0x3FU);
    }
    return value;
}

[[nodiscard]] bool selector(char32_t value) noexcept
{
    return (value >= 0xFE00U && value <= 0xFE0FU) || (value >= 0xE0100U && value <= 0xE01EFU);
}

[[nodiscard]] bool control(char32_t value) noexcept
{
    return value <= 0x1FU || value == 0x7FU;
}

[[nodiscard]] bool zero_width(char32_t value) noexcept
{
    return core::display_width(value) == core::DisplayWidth::zero;
}

// at が手前の 1 文字の続きか（幅 0 で、手前が制御文字でも VS でもない・ADR 0065）。
[[nodiscard]] bool continues(std::string_view text, std::size_t at)
{
    if (at == 0U || at >= text.size() || !zero_width(point_at(text, at)))
    {
        return false;
    }
    const char32_t previous = point_at(text, point_before(text, at));
    return !control(previous) && !selector(previous);
}

[[nodiscard]] bool crlf_text(std::string_view text) noexcept
{
    // 開いた本文の改行の形（ADR 0036）: 最初の LF の手前が CR か、LF が無ければ CRLF。
    const std::size_t first = text.find('\n');
    return first == std::string_view::npos || (first > 0U && text[first - 1U] == '\r');
}

[[nodiscard]] Lines lines_of(std::string_view text, bool crlf)
{
    Lines lines;
    std::size_t begin = 0;
    for (std::size_t at = text.find('\n'); at != std::string_view::npos;
         at = text.find('\n', begin))
    {
        const bool paired = crlf && at > begin && text[at - 1U] == '\r';
        lines.emplace_back(begin, paired ? at - 1U : at, at + 1U);
        begin = at + 1U;
    }
    const bool stripped = crlf && text.size() > begin && text.back() == '\r';
    lines.emplace_back(begin, stripped ? text.size() - 1U : text.size(), text.size());
    return lines;
}

[[nodiscard]] std::size_t points_in(std::string_view text, const Line &line)
{
    const auto content = text.substr(std::get<0>(line), std::get<1>(line) - std::get<0>(line));
    return static_cast<std::size_t>(std::ranges::count_if(content, lead_byte));
}

// 行の内容の n 番目のコードポイントの位置（n が数と同じなら内容の終わり）。
[[nodiscard]] std::size_t nth_point(std::string_view text, const Line &line, std::size_t n)
{
    std::size_t at = std::get<0>(line);
    for (std::size_t seen = 0; seen < n; ++seen)
    {
        at = point_after(text, at);
    }
    return at;
}

// 桁（コードポイントの序数 + 1）から位置。past は「改行の後ろ」の桁（内容の数 + 2）を次の行の
// 始まりとして受けるか（選択の区間の端）。キャレットは最終行の外した単独の CR の後ろだけを受ける。
[[nodiscard]] std::optional<std::size_t> offset_at(std::string_view text, const Lines &lines,
                                                   core::TextPosition position, bool past)
{
    if (position.line.value == 0U || position.line.value > lines.size() ||
        position.column.value == 0U)
    {
        return std::nullopt;
    }
    const Line &line = lines.at(position.line.value - 1U);
    const std::size_t count = points_in(text, line);
    const std::size_t index = position.column.value - 1U;
    if (index <= count)
    {
        return nth_point(text, line, index);
    }
    const bool last = position.line.value == lines.size();
    const bool after_cr = last && std::get<1>(line) != std::get<2>(line);
    if (index == count + 1U && (past || after_cr))
    {
        return std::get<2>(line);
    }
    return std::nullopt;
}

// ---------------------------------------------------------------- 読む口と不変条件 1・2

[[nodiscard]] bool has_lone_return(std::string_view text)
{
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        if (text[at] == '\r' && (at + 1U == text.size() || text[at + 1U] != '\n'))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool has_lone_feed(std::string_view text)
{
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        if (text[at] == '\n' && (at == 0U || text[at - 1U] != '\r'))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] Reason text_violation(std::string_view text, bool crlf, bool tainted)
{
    if (!core::validate_utf8(text).has_value())
    {
        return "1: the text is not valid UTF-8";
    }
    if (crlf && has_lone_feed(text))
    {
        return "1: a CRLF document has a lone LF";
    }
    const bool stray = crlf ? has_lone_return(text) : text.find('\r') != std::string_view::npos;
    return stray && !tainted ? "1: a CR stays although no lone CR was pasted" : Reason{};
}

[[nodiscard]] bool on_boundary(std::string_view text, std::size_t at)
{
    return at <= text.size() && (at == text.size() || lead_byte(text[at])) && !continues(text, at);
}

// frame の選択の区間の最初と最後の端（区間が無ければ nullopt）。
[[nodiscard]] std::expected<std::optional<Positions>, Reason>
selection_ends(std::string_view text, const Lines &lines, const app::EditorFrame &frame)
{
    std::optional<std::size_t> first;
    std::optional<std::size_t> last;
    for (std::size_t index = 0; index < frame.lines.size(); ++index)
    {
        const auto &span = frame.lines.at(index).selection;
        if (span.presence == core::SelectionPresence::absent)
        {
            continue;
        }
        const core::LineNumber number{index + 1U};
        const auto begin = offset_at(text, lines, core::TextPosition{number, span.begin}, true);
        last = offset_at(text, lines, core::TextPosition{number, span.end}, true);
        if (!begin.has_value() || !last.has_value())
        {
            return std::unexpected(Reason{"2: a selection span is outside its line"});
        }
        first = first.has_value() ? first : begin;
    }
    if (!first.has_value() || !last.has_value())
    {
        return std::optional<Positions>{};
    }
    return std::optional<Positions>{Positions{first.value(), last.value()}};
}

[[nodiscard]] std::expected<std::size_t, Reason> anchor_of(std::string_view text,
                                                           const Lines &lines,
                                                           const app::EditorFrame &frame,
                                                           std::size_t caret)
{
    const auto ends = selection_ends(text, lines, frame);
    if (!ends.has_value())
    {
        return std::unexpected(ends.error());
    }
    const std::optional<Positions> &span = ends.value();
    if (!span.has_value())
    {
        return caret;
    }
    const auto [begin, end] = span.value();
    if (begin == end || (caret != begin && caret != end))
    {
        return std::unexpected(Reason{"2: the caret is not an end of the selection"});
    }
    return caret == begin ? end : begin;
}

[[nodiscard]] bool lines_shown(std::string_view text, const Lines &lines,
                               const app::EditorFrame &frame)
{
    if (frame.first_visible.value != 1U || frame.lines.size() != lines.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        const Line &line = lines.at(index);
        if (frame.lines.at(index).text !=
            text.substr(std::get<0>(line), std::get<1>(line) - std::get<0>(line)))
        {
            return false;
        }
    }
    return true;
}

// 今の状態を読み、不変条件 1・2 を見る。
[[nodiscard]] std::expected<Seen, Reason> observed(Editing &editing, const Run &run)
{
    auto &controller = editing.controller();
    std::string text = controller.text().text();
    const bool crlf = crlf_of(run);
    const Reason broken = text_violation(text, crlf, tainted_of(run));
    if (!broken.empty())
    {
        return std::unexpected(broken);
    }
    if ((controller.text().line_ending() == core::LineEnding::crlf) != crlf)
    {
        return std::unexpected(Reason{"1: the document line ending changed"});
    }
    const auto frame = controller.frame();
    const Lines lines = lines_of(text, crlf);
    if (!lines_shown(text, lines, frame))
    {
        return std::unexpected(Reason{"harness: the frame does not show every line as the model"});
    }
    const auto caret = offset_at(text, lines, frame.caret.position, false);
    if (!caret.has_value())
    {
        return std::unexpected(Reason{"2: the caret is outside a line or inside a CRLF pair"});
    }
    const auto anchor = anchor_of(text, lines, frame, caret.value());
    if (!anchor.has_value())
    {
        return std::unexpected(anchor.error());
    }
    if (!on_boundary(text, caret.value()) || !on_boundary(text, anchor.value()))
    {
        return std::unexpected(Reason{"2: the caret or the anchor is inside a character"});
    }
    return Seen{std::move(text), Positions{anchor.value(), caret.value()}};
}

// ---------------------------------------------------------------- 素朴な効果の予測（不変条件 3）

[[nodiscard]] Expected replacing(const Seen &before, std::string_view with)
{
    const auto [anchor, caret] = before.second;
    const std::size_t begin = std::min(anchor, caret);
    const std::size_t end = std::max(anchor, caret);
    if (begin == end && with.empty())
    {
        return Expected{before.first, before.second};
    }
    std::string text = before.first.substr(0, begin);
    text += with;
    text += before.first.substr(end);
    const std::size_t landed = begin + with.size();
    return Expected{std::move(text), Positions{landed, landed}};
}

[[nodiscard]] std::size_t backspace_start(std::string_view text, std::size_t at, bool crlf)
{
    if (at == 0U)
    {
        return 0U;
    }
    std::size_t start = point_before(text, at);
    if (crlf && text[start] == '\n' && start > 0U && text[start - 1U] == '\r')
    {
        --start;
    }
    while (start > 0U && selector(point_at(text, start)) &&
           !control(point_at(text, point_before(text, start))))
    {
        start = point_before(text, start);
    }
    return start;
}

[[nodiscard]] std::size_t delete_end(std::string_view text, std::size_t at, bool crlf)
{
    if (at >= text.size())
    {
        return text.size();
    }
    std::size_t end = point_after(text, at);
    if (crlf && text[at] == '\r' && end < text.size() && text[end] == '\n')
    {
        ++end;
    }
    while (continues(text, end))
    {
        end = point_after(text, end);
    }
    while (!control(point_at(text, at)) && end < text.size() && selector(point_at(text, end)))
    {
        end = point_after(text, end);
    }
    return end;
}

[[nodiscard]] Expected deleting(const Seen &before, core::DeleteDirection direction, bool crlf)
{
    const auto [anchor, caret] = before.second;
    if (anchor != caret)
    {
        return replacing(before, {});
    }
    const bool backward = direction == core::DeleteDirection::backward;
    const std::size_t begin = backward ? backspace_start(before.first, caret, crlf) : caret;
    const std::size_t end = backward ? caret : delete_end(before.first, caret, crlf);
    return replacing(Seen{before.first, Positions{begin, end}}, {});
}

// 貼り付けの揃え（ADR 0055 の決定 1〜3）: CRLF を LF に畳み、LF を文書の改行にする。単独の CR は
// 文字のまま。
[[nodiscard]] std::string pasted_form(std::string_view text, bool crlf)
{
    std::string formed;
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        const bool pair = text[at] == '\r' && at + 1U < text.size() && text[at + 1U] == '\n';
        if (text[at] == '\n')
        {
            formed += crlf ? "\r\n" : "\n";
        }
        else if (!pair)
        {
            formed += text[at];
        }
    }
    return formed;
}

[[nodiscard]] Expected clipboard_effect(Editing &editing, Run &run,
                                        app::ClipboardOperation operation)
{
    const Seen &before = seen_of(run);
    switch (operation)
    {
    case app::ClipboardOperation::copy:
    case app::ClipboardOperation::paste_html:
        return Expected{before.first, before.second};
    case app::ClipboardOperation::cut:
        return replacing(before, {});
    case app::ClipboardOperation::paste:
    case app::ClipboardOperation::paste_plain:
        break;
    }
    const auto content = editing.clipboard().read();
    if (!content.has_value())
    {
        return Expected{before.first, before.second};
    }
    tainted_of(run) = tainted_of(run) || has_lone_return(content.value());
    return replacing(before, pasted_form(content.value(), crlf_of(run)));
}

// 本文を書き換える意図の効果の予測（それ以外は nullopt）。
[[nodiscard]] std::optional<Expected> edit_prediction(Editing &editing, Run &run,
                                                      const app::EditorIntent &intent)
{
    const Seen &before = seen_of(run);
    if (const auto *insert = std::get_if<app::InsertText>(&intent))
    {
        return replacing(before, insert->utf8);
    }
    if (const auto *commit = std::get_if<app::CommitText>(&intent))
    {
        return replacing(before, commit->utf8);
    }
    if (std::holds_alternative<app::NewLine>(intent))
    {
        return replacing(before, crlf_of(run) ? "\r\n" : "\n");
    }
    if (const auto *erase = std::get_if<app::DeleteText>(&intent))
    {
        return deleting(before, erase->direction, crlf_of(run));
    }
    if (const auto *clip = std::get_if<app::ClipboardAction>(&intent))
    {
        return clipboard_effect(editing, run, clip->operation);
    }
    return std::nullopt;
}

// 意図の効果の予測。nullopt は予測しない（undo / redo）。位置の nullopt は着地を予測しない
// （MoveCaret / PlaceCaret）。
[[nodiscard]] std::optional<Expected> predicted(Editing &editing, Run &run,
                                                const app::EditorIntent &intent)
{
    auto edited = edit_prediction(editing, run, intent);
    if (edited.has_value())
    {
        return edited;
    }
    const Seen &before = seen_of(run);
    if (std::holds_alternative<app::SelectAll>(intent))
    {
        return Expected{before.first, Positions{0U, before.first.size()}};
    }
    if (std::holds_alternative<app::CancelSelection>(intent))
    {
        return Expected{before.first, Positions{before.second.second, before.second.second}};
    }
    if (std::holds_alternative<app::HistoryAction>(intent))
    {
        return std::nullopt;
    }
    const bool lands = std::holds_alternative<app::CancelComposition>(intent);
    return Expected{before.first, lands ? std::optional<Positions>{before.second} : std::nullopt};
}

// copy / cut が替え玉に書いたもの。製品の今の規則: 選択が空なら書かない（copy_selection と
// cut_selection は空の範囲で戻る）。書くのは選択の本文そのもの（文書の改行の形・ADR 0055 の
// 決定 4）。
[[nodiscard]] Reason clipboard_violation(Editing &editing, const app::EditorIntent &intent,
                                         const Seen &before, std::size_t writes)
{
    const auto *clip = std::get_if<app::ClipboardAction>(&intent);
    const bool writing = clip != nullptr && (clip->operation == app::ClipboardOperation::copy ||
                                             clip->operation == app::ClipboardOperation::cut);
    const auto [anchor, caret] = before.second;
    const bool selected = anchor != caret;
    if (!writing || !selected)
    {
        return editing.clipboard().writes() == writes ? Reason{}
                                                      : "3: the clipboard is written without a "
                                                        "copy or cut of a selection";
    }
    const std::size_t begin = std::min(anchor, caret);
    const auto content = editing.clipboard().read();
    const bool same =
        content.has_value() &&
        content.value() == before.first.substr(begin, std::max(anchor, caret) - begin);
    return editing.clipboard().writes() == writes + 1U && same
               ? Reason{}
               : "3: copy or cut does not write the selected text once";
}

[[nodiscard]] Reason effect_violation(const std::optional<Expected> &expected, const Seen &after)
{
    if (!expected.has_value())
    {
        return {};
    }
    if (expected.value().first != after.first)
    {
        return "3: the text differs from the naive prediction";
    }
    const auto &positions = expected.value().second;
    return positions.has_value() && positions.value() != after.second
               ? "3: the anchor or the caret differs from the naive prediction"
               : Reason{};
}

// ---------------------------------------------------------------- 歩を流す

[[nodiscard]] Reason intended(Editing &editing, Run &run, const app::EditorIntent &intent)
{
    const Seen before = seen_of(run);
    const auto expected = predicted(editing, run, intent);
    const std::size_t writes = editing.clipboard().writes();
    static_cast<void>(editing.controller().apply(intent));
    const auto after = observed(editing, run);
    if (!after.has_value())
    {
        return after.error();
    }
    if (editing.controller().is_composing())
    {
        return "3: a composition stays after the intent";
    }
    Reason reason = clipboard_violation(editing, intent, before, writes);
    reason = reason.empty() ? effect_violation(expected, after.value()) : reason;
    edited_of(run) =
        !std::holds_alternative<app::HistoryAction>(intent) && after.value().first != before.first;
    seen_of(run) = after.value();
    return reason;
}

[[nodiscard]] Reason composed(Editing &editing, Run &run, const Word &word)
{
    static_cast<void>(editing.controller().apply(app::ComposeText{word.first}));
    if (editing.controller().text().text() != seen_of(run).first ||
        !editing.controller().is_composing())
    {
        return "3: ComposeText changes the text or does not compose";
    }
    if (word.second.has_value())
    {
        return intended(editing, run, app::CommitText{word.second.value()});
    }
    return intended(editing, run, app::CancelComposition{});
}

[[nodiscard]] Reason round_tripped(Editing &editing, Run &run)
{
    const Seen before = seen_of(run);
    static_cast<void>(editing.controller().apply(app::HistoryAction{core::HistoryDirection::undo}));
    const auto undone = observed(editing, run);
    if (!undone.has_value() || undone.value() == before)
    {
        return undone.has_value() ? Reason{} : undone.error();
    }
    static_cast<void>(editing.controller().apply(app::HistoryAction{core::HistoryDirection::redo}));
    const auto redone = observed(editing, run);
    if (!redone.has_value())
    {
        return redone.error();
    }
    seen_of(run) = redone.value();
    if (redone.value().first != before.first)
    {
        return "4: undo then redo changes the text";
    }
    return edited_of(run) && redone.value().second != before.second
               ? "4: undo then redo right after an edit moves the caret"
               : Reason{};
}

[[nodiscard]] Reason stepped(Editing &editing, Run &run, const Action &action)
{
    if (const auto *intent = std::get_if<app::EditorIntent>(&action))
    {
        return intended(editing, run, *intent);
    }
    if (const auto *hold = std::get_if<Hold>(&action))
    {
        editing.clipboard().hold(hold->has_value()
                                     ? Content{hold->value()}
                                     : Content{std::unexpected(app::ClipboardFailure::empty)});
        return {};
    }
    if (const auto *word = std::get_if<Word>(&action))
    {
        return composed(editing, run, *word);
    }
    return round_tripped(editing, run);
}

void press(Editing &editing, core::HistoryDirection direction, std::size_t times)
{
    for (std::size_t index = 0; index < times; ++index)
    {
        static_cast<void>(editing.controller().apply(app::HistoryAction{direction}));
    }
}

// 列の終わりの往復（不変条件 4）。1 歩が積む undo の単位は高々 1 つなので、歩の数 + 2 回で
// 端に届く（同じ本文に置き換える編集は undo しても見た目が変わらないので「変化が無くなるまで」
// では止めない）。先に redo で端まで進めてから undo の前の本文とする。
[[nodiscard]] Reason history_violation(Editing &editing, const Case &input)
{
    const std::size_t presses = input.second.size() + 2U;
    press(editing, core::HistoryDirection::redo, presses);
    const std::string tip = editing.controller().text().text();
    press(editing, core::HistoryDirection::undo, presses);
    if (editing.controller().text().text() != input.first)
    {
        return "4: undoing every edit does not give the opened text";
    }
    press(editing, core::HistoryDirection::redo, presses);
    return editing.controller().text().text() == tip
               ? Reason{}
               : "4: redoing every edit does not give the text before the undo";
}

// 本文を開いて最初の状態を読む（開いた本文が変わらないことも見る）。
[[nodiscard]] std::expected<Run, Reason> opened(Editing &editing, const std::string &body)
{
    editing.files().hold(body);
    static_cast<void>(editing.controller().apply(app::VisibleLines{visible_lines}));
    static_cast<void>(editing.controller().apply(app::OpenDocument{sample_path()}));
    Run run{Seen{}, crlf_text(body), false, false};
    const auto first = observed(editing, run);
    if (!first.has_value())
    {
        return std::unexpected(first.error());
    }
    if (first.value().first != body)
    {
        return std::unexpected(Reason{"harness: opening changes the text"});
    }
    seen_of(run) = first.value();
    return run;
}

[[nodiscard]] Outcome outcome_of(const Case &input)
{
    Editing editing;
    auto start = opened(editing, input.first);
    if (!start.has_value())
    {
        return Outcome{start.error(), Seen{}};
    }
    Run &run = start.value();
    for (const auto &step : input.second)
    {
        const Reason reason = stepped(editing, run, step.second);
        if (!reason.empty())
        {
            return Outcome{reason, seen_of(run)};
        }
    }
    const Seen last = seen_of(run);
    return Outcome{history_violation(editing, input), last};
}

// 範囲の外や 1 文字の途中を指す PlaceCaret を直に送って読んだ破れ（#971 の回帰の組）。
[[nodiscard]] Reason placed_violation(const std::string &body, core::TextPosition position)
{
    Editing editing;
    auto start = opened(editing, body);
    if (!start.has_value())
    {
        return start.error();
    }
    static_cast<void>(
        editing.controller().apply(app::PlaceCaret{position, core::SelectionAnchoring::collapse}));
    const auto after = observed(editing, start.value());
    return after.has_value() ? Reason{} : after.error();
}

[[nodiscard]] Reason case_violation(const Case &input)
{
    return outcome_of(input).first;
}

// ---------------------------------------------------------------- 生成器

[[nodiscard]] std::string escaped(std::string_view text)
{
    std::string shown;
    for (const char unit : text)
    {
        shown += unit == '\r'   ? std::string("\\r")
                 : unit == '\n' ? std::string("\\n")
                 : unit == '\t' ? std::string("\\t")
                                : std::string(1, unit);
    }
    return shown;
}

template <std::size_t N>
[[nodiscard]] std::string drawn(Random &random, const std::array<std::string_view, N> &pool,
                                std::size_t count)
{
    std::string text;
    for (std::size_t index = 0; index < count; ++index)
    {
        text += pool.at(below(random, N));
    }
    return text;
}

[[nodiscard]] std::string random_body(Random &random)
{
    const std::size_t lines = below(random, line_limit);
    const std::string_view newline = below(random, 2U) == 0U ? "\n" : "\r\n";
    std::string body;
    for (std::size_t line = 0; line < lines; ++line)
    {
        body += line == 0U ? std::string_view{} : newline;
        body += drawn(random, body_tokens, below(random, 5U));
    }
    body += lines > 0U && below(random, 3U) == 0U ? newline : std::string_view{};
    return body;
}

[[nodiscard]] Step intent_step(std::string label, app::EditorIntent intent)
{
    return Step{std::move(label), Action{std::move(intent)}};
}

[[nodiscard]] Step make_insert(Random &random, std::size_t)
{
    std::string text = drawn(random, typed_characters, 1U + below(random, 3U));
    return intent_step("insert \"" + escaped(text) + "\"", app::InsertText{text});
}

[[nodiscard]] Step make_new_line(Random &, std::size_t)
{
    return intent_step("new_line", app::NewLine{});
}

[[nodiscard]] Step make_move(Random &random, std::size_t)
{
    const std::size_t motion = below(random, motions.size());
    const std::size_t anchoring = below(random, anchorings.size());
    return intent_step("move " + std::to_string(motion) + "/" + std::to_string(anchoring),
                       app::MoveCaret{motions.at(motion), anchorings.at(anchoring)});
}

[[nodiscard]] Step make_place(Random &random, std::size_t lines)
{
    const std::size_t line = below(random, lines + 4U);
    const std::size_t column = below(random, 16U);
    const std::size_t anchoring = below(random, anchorings.size());
    const bool trailing = below(random, 2U) == 1U;
    return intent_step(
        "place " + std::to_string(line) + ":" + std::to_string(column) + "/" +
            std::to_string(anchoring) + (trailing ? "t" : "l"),
        app::PlaceCaret{core::TextPosition{core::LineNumber{line}, core::Column{column}},
                        anchorings.at(anchoring),
                        trailing ? core::CaretAffinity::trailing : core::CaretAffinity::leading});
}

[[nodiscard]] Step make_delete(Random &random, std::size_t)
{
    const bool backward = below(random, 2U) == 0U;
    return intent_step(backward ? "backspace" : "delete",
                       app::DeleteText{backward ? core::DeleteDirection::backward
                                                : core::DeleteDirection::forward});
}

[[nodiscard]] Step make_select_all(Random &, std::size_t)
{
    return intent_step("select_all", app::SelectAll{});
}

[[nodiscard]] Step make_cancel_selection(Random &, std::size_t)
{
    return intent_step("cancel_selection", app::CancelSelection{});
}

[[nodiscard]] Step make_clipboard(Random &random, std::size_t)
{
    const std::size_t operation = below(random, clipboard_operations.size());
    return intent_step(std::string(clipboard_names.at(operation)),
                       app::ClipboardAction{clipboard_operations.at(operation)});
}

[[nodiscard]] Step make_history(Random &random, std::size_t)
{
    const bool undo = below(random, 2U) == 0U;
    return intent_step(
        undo ? "undo" : "redo",
        app::HistoryAction{undo ? core::HistoryDirection::undo : core::HistoryDirection::redo});
}

[[nodiscard]] Step make_hold(Random &random, std::size_t)
{
    const std::size_t index = below(random, clipboard_texts.size() + 1U);
    if (index == clipboard_texts.size())
    {
        return Step{"hold (empty)", Action{Hold{}}};
    }
    const std::string text(clipboard_texts.at(index));
    return Step{"hold \"" + escaped(text) + "\"", Action{Hold{text}}};
}

[[nodiscard]] core::Composition random_composition(Random &random)
{
    const std::size_t count = 1U + below(random, 4U);
    std::string text = drawn(random, kana, count);
    const std::size_t width = text.size() / count;
    const std::size_t split = width * below(random, count + 1U);
    std::vector<core::CompositionClause> clauses;
    if (split > 0U && split < text.size())
    {
        clauses.push_back(core::CompositionClause{
            core::OffsetRange{core::Offset{0}, core::Offset{split}}, core::ClauseEmphasis::target});
    }
    clauses.push_back(core::CompositionClause{
        core::OffsetRange{core::Offset{clauses.empty() ? 0U : split}, core::Offset{text.size()}},
        clauses.empty() ? core::ClauseEmphasis::target : core::ClauseEmphasis::other});
    const core::Offset cursor{width * below(random, count + 1U)};
    return core::Composition{std::move(text), std::move(clauses), cursor};
}

[[nodiscard]] Step make_word(Random &random, std::size_t)
{
    core::Composition composition = random_composition(random);
    const std::size_t ending = below(random, 3U);
    std::optional<std::string> commit;
    if (ending == 1U)
    {
        commit = composition.utf8;
    }
    else if (ending == 2U)
    {
        commit = drawn(random, typed_characters, 1U + below(random, 2U));
    }
    std::string label =
        "compose \"" + composition.utf8 + "\"@" + std::to_string(composition.cursor.value) +
        " then " +
        (commit.has_value() ? "commit \"" + escaped(commit.value()) + "\"" : std::string("cancel"));
    return Step{std::move(label), Action{Word{std::move(composition), std::move(commit)}}};
}

[[nodiscard]] Step make_round_trip(Random &, std::size_t)
{
    return Step{"undo+redo", Action{std::monostate{}}};
}

using Maker = Step (*)(Random &, std::size_t);
// 行の重みは並べた回数。編集と貼り付けの道を厚く、選択を作る移動を多めにする。
constexpr std::array<Maker, 22> makers{make_insert,
                                       make_insert,
                                       make_insert,
                                       make_new_line,
                                       make_move,
                                       make_move,
                                       make_move,
                                       make_place,
                                       make_place,
                                       make_delete,
                                       make_delete,
                                       make_select_all,
                                       make_cancel_selection,
                                       make_clipboard,
                                       make_clipboard,
                                       make_clipboard,
                                       make_history,
                                       make_hold,
                                       make_hold,
                                       make_word,
                                       make_word,
                                       make_round_trip};

[[nodiscard]] Case random_case(Random &random)
{
    std::string body = random_body(random);
    const std::size_t lines = static_cast<std::size_t>(std::ranges::count(body, '\n')) + 1U;
    const std::size_t steps = step_floor + below(random, step_spread);
    std::vector<Step> input;
    for (std::size_t index = 0; index < steps; ++index)
    {
        input.push_back(makers.at(below(random, makers.size()))(random, lines));
    }
    return Case{std::move(body), std::move(input)};
}

// 破れた組を縮める: 歩を 1 つずつ外し、破れが残るなら外したままにする。
void minimize(Case &input, Check check)
{
    for (std::size_t index = input.second.size(); index-- > 0;)
    {
        Case smaller = input;
        smaller.second.erase(std::next(smaller.second.begin(), static_cast<std::ptrdiff_t>(index)));
        if (!check(smaller).empty())
        {
            input = std::move(smaller);
        }
    }
}

void report(const char *family, Case input, Check check)
{
    minimize(input, check);
    const Reason reason = check(input);
    std::fprintf(stderr, "  %s: %.*s\n    body: \"%s\"\n    steps:", family,
                 static_cast<int>(reason.size()), reason.data(), escaped(input.first).c_str());
    for (const auto &step : input.second)
    {
        std::fprintf(stderr, " [%s]", step.first.c_str());
    }
    std::fprintf(stderr, "\n");
}

[[nodiscard]] std::size_t broken_cases(const char *family, std::uint64_t seed, Check check)
{
    Random random{seed};
    std::size_t broken = 0;
    for (std::size_t index = 0; index < family_cases; ++index)
    {
        const Case input = random_case(random);
        if (!check(input).empty() && broken++ == 0U)
        {
            report(family, input, check);
        }
    }
    return broken;
}

// ---------------------------------------------------------------- 決定性と既知の組

[[nodiscard]] std::vector<std::string> labels_of(const Case &input)
{
    std::vector<std::string> labels{input.first};
    for (const auto &step : input.second)
    {
        labels.push_back(step.first);
    }
    return labels;
}

void verify_determinism()
{
    Random left{9550U};
    Random right{9550U};
    bool same_cases = true;
    bool same_outcomes = true;
    for (std::size_t round = 0; round < 4U; ++round)
    {
        const Case first = random_case(left);
        same_cases = same_cases && labels_of(first) == labels_of(random_case(right));
        same_outcomes = same_outcomes && outcome_of(first) == outcome_of(first);
    }
    expect(same_cases, "the editing generator gives the same cases for the same seed");
    expect(same_outcomes, "the same case gives the same last text and positions twice");
}

[[nodiscard]] Step held(std::string_view text)
{
    return Step{"hold", Action{Hold{std::string(text)}}};
}

[[nodiscard]] Step pasted()
{
    return intent_step("paste", app::ClipboardAction{app::ClipboardOperation::paste});
}

[[nodiscard]] Step to_end()
{
    return intent_step("move document_end", app::MoveCaret{core::CaretMotion::document_end,
                                                           core::SelectionAnchoring::collapse});
}

[[nodiscard]] Step line_start()
{
    return intent_step("move line_start", app::MoveCaret{core::CaretMotion::line_start,
                                                         core::SelectionAnchoring::collapse});
}

[[nodiscard]] Step erased(core::DeleteDirection direction)
{
    return intent_step("delete", app::DeleteText{direction});
}

[[nodiscard]] std::string last_text(const Case &input)
{
    const Outcome outcome = outcome_of(input);
    return outcome.first.empty() ? outcome.second.first : std::string("(broken)");
}

// 揃えと 1 文字の回帰の組（貼り付けの改行・Backspace の VS と結合文字・Delete の結合文字・
// CRLF の対）。
void verify_known_cases()
{
    // #955 の 1 倍の走りで見つけて最小化した組（#971 で直した）: 本文「か + 結合濁点」で
    // PlaceCaret 1:2 は 1 文字の途中を指すが、キャレットはその文字の先頭に置かれる。
    expect(placed_violation("か\u3099", core::TextPosition{core::LineNumber{1}, core::Column{2}})
               .empty(),
           "PlaceCaret inside a combined character rests at its base");
    // 50 倍の走りで見つけて最小化した組（#971 で直した）: CRLF の本文「a\r\nb」で行 0 の
    // PlaceCaret 0:3 は行 1 へ切り詰め、桁を行の終わり（CR の前）で止める。
    expect(placed_violation("a\r\nb", core::TextPosition{core::LineNumber{0}, core::Column{3}})
               .empty(),
           "PlaceCaret on line 0 clamps to line 1 and stops before the CRLF pair");
    expect(last_text(Case{"a\nb", {held("x\r\ny\rz"), pasted()}}) == "x\ny\rza\nb",
           "pasting CRLF into an LF document folds the pair and keeps a lone CR");
    expect(last_text(Case{"a\r\nb", {held("p\nq"), pasted()}}) == "p\r\nqa\r\nb",
           "pasting LF into a CRLF document gives CRLF");
    expect(last_text(Case{"葛\U000E0100", {to_end(), erased(core::DeleteDirection::backward)}})
               .empty(),
           "Backspace after a variation selector removes it with its base");
    expect(last_text(Case{"e\u0301", {to_end(), erased(core::DeleteDirection::backward)}}) == "e",
           "Backspace after a combining mark removes only the mark");
    expect(last_text(Case{"e\u0301x", {erased(core::DeleteDirection::forward)}}) == "x",
           "Delete removes a base with its combining mark");
    expect(last_text(
               Case{"a\r\nb", {to_end(), line_start(), erased(core::DeleteDirection::backward)}}) ==
               "ab",
           "Backspace at a line start of a CRLF document removes the pair");
    expect(!effect_violation(Expected{"x", std::nullopt}, Seen{"y", Positions{0U, 0U}}).empty() &&
               !text_violation("a\nb", true, false).empty() &&
               !text_violation("a\rb", false, false).empty(),
           "the checks catch a wrong text, a lone LF in CRLF and a CR in LF");
}
} // namespace

void verify_editing_property_scope()
{
    verify_determinism();
    verify_known_cases();
    expect(broken_cases("normal-mode editing keeps its invariants", 9551U, case_violation) == 0U,
           "random normal-mode edits keep UTF-8, the line ending, caret boundaries, the naive "
           "effects and the undo round trip");
}
} // namespace nenenib::tests
