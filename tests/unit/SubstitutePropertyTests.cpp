// scope `--substitute-property` の性質の試験（#1291・ADR 0174 の決定 7・ADR 0032 / 0053・ADR
// 0042）。 固定 seed の決定的な生成器で作った本文（ASCII・多バイト・結合文字・Tab・空行・LF か
// CRLF・末尾の 改行の有無）と pattern と置換文字列（`&` と `\` を含む・どちらも literal
// として入る）で、次を 確かめる。
//   1. literal の語（literal_pattern で magic の文字を逃がしたもの・大小を区別する / しない）の
//      substituted_all の Edit を本文に当てた結果が、素朴な模型（行を '\n' で切り、CRLF の文書は
//      行末の '\r' を外し、各行で語のバイト列を左から重ならないように探して置き換える）と同じ。
//      当たりの始まりと終わりは Vim の 1 文字の境（行頭か、結合文字が始まらない位置・ADR
//      0053・#1146）。 大小を区別しない模型は ASCII の英字だけを畳む（生成する本文の非 ASCII
//      は大小を持たない）。
//   2. magic の部分集合（`.` `*` `[ab]` `[^a]` `^` `$` `\<` `\>` `\s` `\d` の断片の列）の
//      substituted_all が、行ごとに VimPattern::matched を左から歩いた当たりの列（Vim の do_sub の
//      模型・前の当たりに続く長さ 0 の当たりは数えずに 1 code point 進む・ADR 0168 の決定 5）を
//      素朴に置き換えた再構成と同じで、長さ 0 の当たりで止まる（返ってくる）。当たりが無ければ
//      no_match。Edit の removed は本文の その位置のバイト列で、restore は at。 1 と 2
//      の上限の模型（#1376）: 上限は置き換えた後の本文の大きさの 1 つ下・ちょうど・1 つ上・
//      製品の上限を本文の長さの 4 の剰余で選び、模型の本文が上限を超える件（当たりがあるとき）だけ
//      too_large。当たりが無ければ上限に依らず no_match。
//   3. 通常モードの入口（Ctrl+H → 語 → Enter → 置換文字列 → Ctrl+Enter）で全件を置き換えた本文が
//      1 の模型と同じで、undo 1 回で開いた本文に、続く redo 1 回で置換後の本文に戻る。
// 回数は Debug（ASan）で scope 全体が 1 秒程度に終わる値。生成器は std::mt19937_64 の剰余だけを
// 使い、分布の型は使わない。時計と環境変数を読まない。
#include "BeginReplace.hpp"
#include "CancelCommand.hpp"
#include "CommandText.hpp"
#include "DocumentByteLimit.hpp"
#include "Edit.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "LineEnding.hpp"
#include "LiteralPattern.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "SubmitCommand.hpp"
#include "SubstituteAll.hpp"
#include "SubstituteFailure.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimPattern.hpp"
#include "VimPatternCase.hpp"
#include "VimPatternMatch.hpp"
#include "VimSearchDirection.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::VimPattern;
using core::VimPatternCase;
using core::VimSearchDirection;
using Random = std::mt19937_64;
// 本文の中の当たり（半開区間のバイト位置）。
using Span = std::pair<std::size_t, std::size_t>;
using Spans = std::vector<Span>;
// 本文・pattern（語か magic）・置換文字列。
using Case = std::pair<std::string, std::pair<std::string, std::string>>;

constexpr std::size_t rounds = 1U;
constexpr std::size_t literal_cases = 500U * rounds;
constexpr std::size_t magic_cases = 500U * rounds;
constexpr std::size_t controller_cases = 60U * rounds;
constexpr std::size_t most_lines = 8U;
constexpr std::size_t most_pieces = 10U;

// 本文の断片。結合文字（U+0301・U+3099）は前の文字の一部（ADR 0053）。大文字の非 ASCII は入れない
// （大小を区別しない模型が ASCII だけを畳むため）。
constexpr std::array<std::string_view, 20> text_pieces{{
    "a",
    "b",
    "ab",
    "A",
    "B",
    " ",
    "\t",
    "aa",
    "ba",
    "\xC3\xA9",
    "e",
    "\xCC\x81",
    "\xE3\x81\x8B",
    "\xE3\x82\x99",
    "\xE6\x97\xA5",
    "&",
    "\\",
    ".",
    "*",
    "1",
}};

// 語の断片（magic の文字も literal_pattern が逃がす）。
constexpr std::array<std::string_view, 16> word_pieces{{
    "a",
    "b",
    "ab",
    "A",
    " ",
    "\xC3\xA9",
    "e",
    "\xCC\x81",
    "\xE3\x81\x8B",
    "\xE6\x97\xA5",
    "&",
    "\\",
    ".",
    "*",
    "[",
    "~",
}};

// magic の断片（ADR 0032 の決定 4 の部分集合）。
constexpr std::array<std::string_view, 17> magic_pieces{{
    "a",
    "b",
    ".",
    "a*",
    ".*",
    "b*",
    "[ab]",
    "[^a]",
    "^",
    "$",
    "\\<",
    "\\>",
    "\\s",
    "\\d",
    "\xC3\xA9",
    "e",
    "x*",
}};

// 置換文字列の断片（`&` `\` `\n` `\1` も literal として入る・決定 7）。
constexpr std::array<std::string_view, 9> replacement_pieces{{
    "&",
    "\\",
    "\\n",
    "\\1",
    "x",
    "\xE3\x81\x82",
    "~",
    "\\&",
    "Z",
}};

// 結合文字（本文の断片に入るもの）。直前の位置は Vim の 1 文字の境でない。
constexpr std::array<std::string_view, 2> combining{{"\xCC\x81", "\xE3\x82\x99"}};

[[nodiscard]] std::size_t below(Random &random, std::size_t limit)
{
    return static_cast<std::size_t>(random() % limit);
}

template <std::size_t Size>
[[nodiscard]] std::string pieces_of(Random &random, const std::array<std::string_view, Size> &table,
                                    std::size_t lowest, std::size_t highest)
{
    std::string text;
    const std::size_t count = lowest + below(random, highest - lowest + 1U);
    for (std::size_t index = 0; index < count; ++index)
    {
        text += table.at(below(random, table.size()));
    }
    return text;
}

// 0〜8 行・LF か CRLF・末尾の改行は有ったり無かったり。
[[nodiscard]] std::string random_text(Random &random, bool crlf, bool with_tab)
{
    const std::string_view newline = crlf ? "\r\n" : "\n";
    std::string text;
    const std::size_t lines = below(random, most_lines + 1U);
    for (std::size_t line = 0; line < lines; ++line)
    {
        text += line == 0 ? std::string{} : std::string{newline};
        const bool empty = below(random, 5U) == 0;
        text += empty ? std::string{} : pieces_of(random, text_pieces, 1U, most_pieces);
    }
    if (lines > 0 && below(random, 3U) == 0)
    {
        text += newline;
    }
    if (!with_tab)
    {
        std::erase(text, '\t');
    }
    return text;
}

[[nodiscard]] char folded(char byte, bool ignore)
{
    return ignore && byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte - 'A' + 'a') : byte;
}

// at が Vim の 1 文字の境（行頭か、そこから結合文字が始まらない）か。
[[nodiscard]] bool on_boundary(std::string_view line, std::size_t at)
{
    // 行頭の結合文字は前の文字が無いので、それだけで 1 文字（vim_character_continues の注記）。
    bool boundary = true;
    for (const std::string_view mark : combining)
    {
        boundary = boundary && !line.substr(at).starts_with(mark);
    }
    return at == 0 || boundary;
}

[[nodiscard]] bool same_at(std::string_view line, std::size_t at, std::string_view word,
                           bool ignore)
{
    if (at + word.size() > line.size())
    {
        return false;
    }
    bool same = true;
    for (std::size_t index = 0; index < word.size(); ++index)
    {
        same = same && folded(line.at(at + index), ignore) == folded(word.at(index), ignore);
    }
    return same;
}

// 1 行の中の語の当たり（左から・重ならない・両端が 1 文字の境）。
[[nodiscard]] Spans word_spans(std::string_view line, std::string_view word, bool ignore)
{
    Spans spans;
    std::size_t at = 0;
    while (at + word.size() <= line.size())
    {
        const bool hit = same_at(line, at, word, ignore) && on_boundary(line, at) &&
                         on_boundary(line, at + word.size());
        if (hit)
        {
            spans.emplace_back(at, at + word.size());
        }
        at = hit ? at + word.size() : at + 1U;
    }
    return spans;
}

// 1 行の中の pattern の置き換える当たり（Vim の do_sub の模型・ADR 0168 の決定 5）。matched
// を左から歩き、前の当たりの終わりにぴったり続く長さ 0 の当たりは数えずに Vim の 1 文字（結合文字
// まで・mb_ptr2len）進み、行の終わりに着いたら止める（`:s/x*/-/g` は "abc" を "-a-b-c" に・Vim 9.1
// の実測）。結合文字の手前の 1 code point だけ進めると、行末の `.` + U+0301 の後ろにもう 1 つ
// 長さ 0 の当たりを数えてしまう。
[[nodiscard]] Spans pattern_spans(std::string_view line, const VimPattern &pattern)
{
    Spans spans;
    std::size_t at = 0;
    std::optional<std::size_t> previous;
    while (true)
    {
        const auto found = pattern.matched(line, at);
        if (!found.has_value())
        {
            return spans;
        }
        const auto [begin, end] = std::pair{found.value().begin, found.value().end};
        const bool skipped = previous == at && end == at;
        if (!skipped)
        {
            spans.emplace_back(begin, end);
            previous = end;
        }
        at = skipped ? core::vim_character_end(line, core::Offset{at}).value : end;
        if (at >= line.size())
        {
            return spans;
        }
    }
}

// 本文を模型の行に切り、行ごとの当たりを本文の位置に直して並べる。
template <typename Find>
[[nodiscard]] Spans text_spans(std::string_view text, bool crlf, const Find &find)
{
    Spans spans;
    std::size_t start = 0;
    while (true)
    {
        const std::size_t newline = text.find('\n', start);
        const std::size_t stop = newline == std::string_view::npos ? text.size() : newline;
        std::string_view line = text.substr(start, stop - start);
        if (crlf && line.ends_with('\r'))
        {
            line.remove_suffix(1);
        }
        for (const auto &[begin, end] : find(line))
        {
            spans.emplace_back(start + begin, start + end);
        }
        if (newline == std::string_view::npos)
        {
            return spans;
        }
        start = newline + 1U;
    }
}

// 当たりの列を左から置き換えた本文。
[[nodiscard]] std::string rebuilt(std::string_view text, const Spans &spans,
                                  std::string_view replacement)
{
    std::string result;
    std::size_t copied = 0;
    for (const auto &[begin, end] : spans)
    {
        result.append(text.substr(copied, begin - copied));
        result.append(replacement);
        copied = end;
    }
    result.append(text.substr(copied));
    return result;
}

// Edit を当てた本文。removed が本文のその位置のバイト列でなければ値なし。
[[nodiscard]] std::optional<std::string> edited(std::string_view text, const core::Edit &edit)
{
    const std::size_t at = edit.at.value;
    if (at > text.size() || text.substr(at, edit.removed.size()) != edit.removed ||
        edit.restore.value != at)
    {
        return std::nullopt;
    }
    return std::string{text.substr(0, at)} + edit.inserted +
           std::string{text.substr(at + edit.removed.size())};
}

// 件 input の上限（#1376）。置き換えた後の本文の大きさ expected の 1 つ下・ちょうど・1 つ上・
// 製品の上限を本文の長さの 4 の剰余で選ぶ。生成器の乱数は使わない（ほかの件の列を変えない）。
[[nodiscard]] std::size_t limit_for(const Case &input, std::size_t expected)
{
    const std::array<std::size_t, 4> limits{expected > 0 ? expected - 1U : 0U, expected,
                                            expected + 1U, core::maximum_document_bytes};
    return limits.at(input.first.size() % limits.size());
}

// substituted_all の答えが当たりの列 spans の再構成と同じか。上限は limit_for で、模型の本文が
// 上限を超えるなら too_large、当たりが無ければ no_match。
[[nodiscard]] bool substitution_matches(const Case &input, const core::TextBuffer &buffer,
                                        const VimPattern &pattern, const Spans &spans)
{
    const std::string &text = input.first;
    const std::string &replacement = input.second.second;
    const std::string expected = rebuilt(text, spans, replacement);
    const std::size_t limit = limit_for(input, expected.size());
    const auto edit = core::substituted_all(buffer, pattern, replacement, limit);
    if (spans.empty() || expected.size() > limit)
    {
        const core::SubstituteFailure failure =
            spans.empty() ? core::SubstituteFailure::no_match : core::SubstituteFailure::too_large;
        return !edit.has_value() && edit.error() == failure;
    }
    if (!edit.has_value())
    {
        return false;
    }
    const std::optional<std::string> result = edited(text, edit.value());
    return result.has_value() && edit.value().at.value == spans.front().first &&
           result.value() == expected;
}

void print_bytes(const char *label, std::string_view input)
{
    const std::size_t shown = input.size() < 160U ? input.size() : 160U;
    std::fprintf(stderr, "    %s (%zu bytes):", label, input.size());
    for (std::size_t index = 0; index < shown; ++index)
    {
        std::fprintf(stderr, " %02x",
                     static_cast<unsigned>(static_cast<unsigned char>(input[index])));
    }
    std::fprintf(stderr, "\n");
}

void report(const char *family, std::size_t index, const Case &input)
{
    std::fprintf(stderr, "  %s: case %zu\n", family, index);
    print_bytes("text", input.first);
    print_bytes("pattern", input.second.first);
    print_bytes("replacement", input.second.second);
}

// 破れの数を 1 つ増やし、最初の 1 つだけ入力を出す。
[[nodiscard]] std::size_t counted_broken(std::size_t broken, const char *family, std::size_t index,
                                         const Case &input)
{
    if (broken == 0)
    {
        report(family, index, input);
    }
    return broken + 1U;
}

[[nodiscard]] bool crlf_of(const core::TextBuffer &buffer)
{
    return buffer.line_ending() == core::LineEnding::crlf;
}

[[nodiscard]] std::optional<VimPattern> literal_pattern_of(std::string_view word, bool ignore)
{
    auto parsed = VimPattern::parse(core::literal_pattern(word, VimSearchDirection::forward),
                                    VimSearchDirection::forward,
                                    ignore ? VimPatternCase::ignore : VimPatternCase::sensitive);
    if (!parsed.has_value())
    {
        return std::nullopt;
    }
    return std::move(parsed).value();
}

[[nodiscard]] bool literal_holds(const Case &input, bool ignore)
{
    const std::string &text = input.first;
    const std::string &word = input.second.first;
    const auto pattern = literal_pattern_of(word, ignore);
    const auto buffer = core::TextBuffer::from_utf8(text);
    if (!pattern.has_value() || !buffer.has_value())
    {
        return false;
    }
    const Spans spans =
        text_spans(text, crlf_of(buffer.value()), [&word, ignore](std::string_view line)
                   { return word_spans(line, word, ignore); });
    return substitution_matches(input, buffer.value(), pattern.value(), spans);
}

void verify_literal_words()
{
    Random random{129111U};
    std::size_t broken = 0;
    for (std::size_t index = 0; index < literal_cases; ++index)
    {
        const bool crlf = below(random, 2U) == 0;
        std::string text = random_text(random, crlf, true);
        std::string word = pieces_of(random, word_pieces, 1U, 3U);
        std::string replacement = pieces_of(random, replacement_pieces, 0U, 3U);
        const Case input{std::move(text), {std::move(word), std::move(replacement)}};
        const bool ignore = below(random, 3U) == 0;
        if (!literal_holds(input, ignore))
        {
            broken =
                counted_broken(broken, ignore ? "literal (ignore case)" : "literal", index, input);
        }
    }
    expect(broken == 0, "substituting a literal word equals rebuilding the text from its matches");
}

// magic の列。parse が断ったら値なし（数えて報告する）。
[[nodiscard]] std::optional<bool> magic_holds(const Case &input)
{
    const std::string &text = input.first;
    const std::string &source = input.second.first;
    auto parsed = VimPattern::parse(source, VimSearchDirection::forward, VimPatternCase::sensitive);
    const auto buffer = core::TextBuffer::from_utf8(text);
    if (!parsed.has_value() || !buffer.has_value())
    {
        return std::nullopt;
    }
    const VimPattern &pattern = parsed.value();
    const Spans spans = text_spans(text, crlf_of(buffer.value()), [&pattern](std::string_view line)
                                   { return pattern_spans(line, pattern); });
    return substitution_matches(input, buffer.value(), pattern, spans);
}

void verify_magic_patterns()
{
    Random random{129112U};
    std::size_t broken = 0;
    std::size_t refused = 0;
    for (std::size_t index = 0; index < magic_cases; ++index)
    {
        const bool crlf = below(random, 2U) == 0;
        std::string text = random_text(random, crlf, true);
        std::string source = pieces_of(random, magic_pieces, 1U, 3U);
        std::string replacement = pieces_of(random, replacement_pieces, 0U, 3U);
        const Case input{std::move(text), {std::move(source), std::move(replacement)}};
        const std::optional<bool> held = magic_holds(input);
        refused += held.has_value() ? 0U : 1U;
        if (held.has_value() && !held.value())
        {
            broken = counted_broken(broken, "magic", index, input);
        }
    }
    expect(broken == 0, "substituting a magic pattern equals rebuilding the text from the matches "
                        "walked left to right, and empty matches end");
    expect(refused * 4U < magic_cases, "most generated magic patterns parse");
}

void open_ordinary(Editing &editing, std::string text)
{
    editing.files().hold(Bytes{std::move(text)});
    applied(editing.controller(), application::VisibleLines{vim_visible_lines});
    applied(editing.controller(), application::OpenDocument{sample_path()});
}

// Ctrl+H → 語 → Enter → 置換文字列 → Ctrl+Enter → 閉じる → undo → redo の本文。
[[nodiscard]] std::array<std::string, 3> replaced_through_controller(const Case &input)
{
    const std::string &word = input.second.first;
    const std::string &replacement = input.second.second;
    Editing session;
    open_ordinary(session, input.first);
    application::EditorController &controller = session.controller();
    static_cast<void>(controller.apply_frame(application::BeginReplace{}));
    static_cast<void>(controller.apply_frame(application::CommandText{word}));
    static_cast<void>(controller.apply_frame(application::SubmitCommand{}));
    static_cast<void>(controller.apply_frame(application::CommandText{replacement}));
    static_cast<void>(controller.apply_frame(application::SubmitCommand{true}));
    static_cast<void>(controller.apply_frame(application::CancelCommand{}));
    std::array<std::string, 3> seen{controller.text().text(), std::string{}, std::string{}};
    applied(controller, application::HistoryAction{core::HistoryDirection::undo});
    seen.at(1) = controller.text().text();
    applied(controller, application::HistoryAction{core::HistoryDirection::redo});
    seen.at(2) = controller.text().text();
    return seen;
}

[[nodiscard]] bool controller_holds(const Case &input)
{
    const std::string &text = input.first;
    const std::string &word = input.second.first;
    const std::string &replacement = input.second.second;
    const auto buffer = core::TextBuffer::from_utf8(text);
    if (!buffer.has_value())
    {
        return false;
    }
    const Spans spans = text_spans(text, crlf_of(buffer.value()), [&word](std::string_view line)
                                   { return word_spans(line, word, false); });
    const std::string expected = rebuilt(text, spans, replacement);
    const auto [after, undone, redone] = replaced_through_controller(input);
    return after == expected && undone == text && redone == expected;
}

void verify_controller_replace_all()
{
    Random random{129113U};
    std::size_t broken = 0;
    for (std::size_t index = 0; index < controller_cases; ++index)
    {
        const bool crlf = below(random, 2U) == 0;
        std::string text = random_text(random, crlf, false);
        std::string word = pieces_of(random, word_pieces, 1U, 2U);
        std::string replacement = pieces_of(random, replacement_pieces, 0U, 2U);
        const Case input{std::move(text), {std::move(word), std::move(replacement)}};
        if (!controller_holds(input))
        {
            broken = counted_broken(broken, "controller", index, input);
        }
    }
    expect(broken == 0, "Ctrl+Enter replaces every match like the model and one undo and one redo "
                        "go back and forth");
}

void verify_generators_are_deterministic()
{
    Random first{129114U};
    Random second{129114U};
    expect(random_text(first, true, true) == random_text(second, true, true) &&
               pieces_of(first, magic_pieces, 1U, 3U) == pieces_of(second, magic_pieces, 1U, 3U),
           "the text and pattern generators are deterministic for a fixed seed");
}
} // namespace

void verify_substitute_property_scope()
{
    verify_generators_are_deterministic();
    verify_literal_words();
    verify_magic_patterns();
    verify_controller_replace_all();
}
} // namespace nenenib::tests
