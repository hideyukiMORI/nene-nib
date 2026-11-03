// scope `--lsp-diagnostics` の契約（#995・ADR 0161 の決定 2・3・4・8・ADR 0042）。core の診断の
// 3 本（publishDiagnostics → 文書の値・編集でのずらし・見えている 1 行の下線の区間）と Range の
// 写しを当てる。位置は ASCII・日本語・絵文字（UTF-16 の代理対）・CRLF・行と桁の外、上限は
// 1,000 件、ずらしは編集の前・中・後ろ・境目。最後に固定 seed の性質の試験を 1 本流す。
#include "Diagnostic.hpp"
#include "DiagnosticEdits.hpp"
#include "DiagnosticSeverity.hpp"
#include "DiagnosticSpan.hpp"
#include "DiagnosticsCount.hpp"
#include "Edit.hpp"
#include "JsonParse.hpp"
#include "JsonValue.hpp"
#include "LspDiagnostics.hpp"
#include "LspDiagnosticsFailure.hpp"
#include "LspNotification.hpp"
#include "LspPosition.hpp"
#include "LspPositionEncoding.hpp"
#include "LspRange.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "PublishedDiagnostics.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
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
using core::Diagnostic;
using core::DiagnosticSeverity;
using core::DiagnosticSpan;
using core::Edit;
using core::JsonValue;
using core::LspDiagnosticsFailure;
using core::LspPositionEncoding;
using core::LspRange;
using core::Offset;
using core::OffsetRange;
using Published = std::expected<core::PublishedDiagnostics, LspDiagnosticsFailure>;
using Random = std::mt19937_64;
// 本文・Range・符号化と、写したバイト位置の区間（値なしは捨てる）。
using Bytes = std::optional<std::pair<std::size_t, std::size_t>>;
using Placed =
    std::pair<std::pair<std::string_view, LspRange>, std::pair<LspPositionEncoding, Bytes>>;
// 編集の前の診断の区間・編集（at・消すバイト数・入れる本文）と、ずらした区間（値なしは消える）。
using Shift = std::pair<std::pair<std::pair<std::size_t, std::size_t>,
                                  std::pair<std::pair<std::size_t, std::size_t>, std::string_view>>,
                        Bytes>;

constexpr auto utf8 = LspPositionEncoding::utf8;
constexpr auto utf16 = LspPositionEncoding::utf16;
constexpr std::uint64_t property_seed = 995;
constexpr std::size_t property_rounds = 400;

[[nodiscard]] constexpr LspRange range_of(std::size_t first_line, std::size_t first_character,
                                          std::size_t last_line, std::size_t last_character)
{
    return LspRange{.start = {.line = first_line, .character = first_character},
                    .end = {.line = last_line, .character = last_character}};
}

[[nodiscard]] constexpr Bytes bytes(std::size_t begin, std::size_t end)
{
    return std::pair{begin, end};
}

// Range の写しの表。日本語は 1 文字が UTF-8 で 3 バイト・UTF-16 で 1 単位、絵文字は 4 バイト・
// 2 単位。CRLF の行末は `\r` の前。
constexpr std::array<Placed, 15> placed{{
    {{"int x = y;\n", range_of(0, 8, 0, 9)}, {utf16, bytes(8, 9)}},
    {{"int x = y;\n", range_of(0, 8, 0, 9)}, {utf8, bytes(8, 9)}},
    {{"日本語x\n", range_of(0, 1, 0, 3)}, {utf16, bytes(3, 9)}},
    {{"日本語x\n", range_of(0, 3, 0, 9)}, {utf8, bytes(3, 9)}},
    {{"a😀b\n", range_of(0, 1, 0, 3)}, {utf16, bytes(1, 5)}},
    {{"a😀b\n", range_of(0, 2, 0, 3)}, {utf16, bytes(5, 5)}},
    {{"ab\r\ncd\r\n", range_of(0, 1, 1, 1)}, {utf16, bytes(1, 5)}},
    {{"ab\r\ncd\r\n", range_of(0, 0, 0, 99)}, {utf16, bytes(0, 2)}},
    {{"ab\ncd", range_of(1, 0, 7, 0)}, {utf16, bytes(3, 5)}},
    {{"ab\ncd", range_of(2, 0, 2, 1)}, {utf16, bytes(5, 5)}},
    {{"ab\ncd", range_of(3, 0, 0, 1)}, {utf16, std::nullopt}},
    {{"ab\ncd", range_of(1, 1, 0, 1)}, {utf16, std::nullopt}},
    {{"ab\ncd", range_of(0, 5, 0, 1)}, {utf16, std::nullopt}},
    {{"", range_of(0, 0, 0, 4)}, {utf16, bytes(0, 0)}},
    {{"ab\n", range_of(1, 0, 1, 0)}, {utf16, bytes(3, 3)}},
}};

// ずらしの表。本文は 20 バイトの ASCII を思い浮かべる（位置だけを見るので本文は要らない）。
constexpr std::array<Shift, 12> shifts{{
    // 前: 編集より前で終わる診断はそのまま（ちょうど at で終わっても伸ばさない）。
    {{{2, 4}, {{6, 0}, "xyz"}}, bytes(2, 4)},
    {{{2, 6}, {{6, 2}, "xyz"}}, bytes(2, 6)},
    // 後ろ: 削除と挿入の差だけずらす（at から始まる診断は純粋な挿入の後ろへ）。
    {{{10, 12}, {{2, 3}, "a"}}, bytes(8, 10)},
    {{{6, 8}, {{6, 0}, "xyz"}}, bytes(9, 11)},
    {{{8, 9}, {{6, 2}, ""}}, bytes(6, 7)},
    // 中: 消した範囲と重なる・純粋な挿入が内側に入る診断は消す。
    {{{4, 8}, {{6, 0}, "x"}}, std::nullopt},
    {{{4, 8}, {{2, 3}, ""}}, std::nullopt},
    {{{4, 8}, {{7, 4}, "y"}}, std::nullopt},
    {{{4, 8}, {{0, 20}, ""}}, std::nullopt},
    // 空の診断: 消した範囲の内側なら消え、at にあれば挿入の後ろへ、範囲の端なら残る。
    {{{6, 6}, {{4, 4}, ""}}, std::nullopt},
    {{{6, 6}, {{6, 0}, "ab"}}, bytes(8, 8)},
    {{{8, 8}, {{4, 4}, "z"}}, bytes(5, 5)},
}};

[[nodiscard]] core::TextBuffer text_of(std::string_view text)
{
    return buffer_of(text);
}

[[nodiscard]] core::LspNotification notification_of(std::string_view params)
{
    const auto parsed = core::parse_json(params);
    expect(parsed.has_value(), "the publishDiagnostics params are valid JSON");
    return core::LspNotification{.method = std::string{core::lsp_publish_diagnostics_method},
                                 .params = parsed.value_or(JsonValue{})};
}

[[nodiscard]] Published published_of(std::string_view params, std::string_view text)
{
    return core::lsp_published_diagnostics(notification_of(params), text_of(text), utf16);
}

[[nodiscard]] Diagnostic diagnostic_at(std::size_t begin, std::size_t end,
                                       DiagnosticSeverity severity)
{
    return Diagnostic{
        .range = OffsetRange{Offset{begin}, Offset{end}}, .severity = severity, .message = "m"};
}

[[nodiscard]] DiagnosticSpan span_of(std::size_t begin, std::size_t end,
                                     DiagnosticSeverity severity)
{
    return DiagnosticSpan{
        .begin = core::Column{begin}, .end = core::Column{end}, .severity = severity};
}

void verify_ranges()
{
    for (const auto &[input, expected] : placed)
    {
        const std::optional<OffsetRange> range =
            core::lsp_range_to_offsets(text_of(input.first), input.second, expected.first);
        const bool same =
            expected.second.has_value()
                ? range.has_value() &&
                      range.value() == OffsetRange{Offset{expected.second.value().first},
                                                   Offset{expected.second.value().second}}
                : !range.has_value();
        expect(same,
               "an LSP range maps onto the text bytes (lines and columns outside are clamped)");
    }
}

void verify_published()
{
    const Published one = published_of(
        R"({"uri":"file:///a.cpp","version":7,"diagnostics":[)"
        R"({"range":{"start":{"line":0,"character":1},"end":{"line":0,"character":3}},)"
        R"("severity":2,"message":"unused\nnote: here","source":"clangd","code":"x",)"
        R"("relatedInformation":[]},)"
        R"({"range":{"start":{"line":1,"character":0},"end":{"line":1,"character":1}},)"
        R"("message":"no severity\r\nsecond"}]})",
        "日本語x\nab");
    expect(one.has_value() && one.value().uri == "file:///a.cpp" &&
               one.value().version == std::optional<std::uint64_t>{7} &&
               one.value().count == core::DiagnosticsCount::complete,
           "a publishDiagnostics keeps its uri and version");
    const std::vector<Diagnostic> expected{
        Diagnostic{OffsetRange{Offset{3}, Offset{9}}, DiagnosticSeverity::warning, "unused"},
        Diagnostic{OffsetRange{Offset{11}, Offset{12}}, DiagnosticSeverity::error, "no severity"}};
    expect(one.has_value() && one.value().diagnostics == expected,
           "each diagnostic maps its range, reads severity (absent is error) and keeps one line");
    const Published bare = published_of(R"({"uri":"u","version":null,"diagnostics":[]})", "a");
    expect(bare.has_value() && !bare.value().version.has_value() &&
               bare.value().diagnostics.empty(),
           "a null version reads as no version");
    const Published unversioned = published_of(R"({"uri":"u","diagnostics":[]})", "a");
    expect(unversioned.has_value() && !unversioned.value().version.has_value(),
           "an absent version reads as no version");
}

void verify_dropped_entries()
{
    const Published kept = published_of(
        R"({"uri":"u","diagnostics":[)"
        R"({"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},"severity":5,"message":"x"},)"
        R"({"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},"severity":0,"message":"x"},)"
        R"({"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}}},)"
        R"({"severity":1,"message":"x"},)"
        R"({"range":{"start":{"line":9,"character":0},"end":{"line":9,"character":1}},"message":"x"},)"
        R"({"range":{"start":{"line":0,"character":-1},"end":{"line":0,"character":1}},"message":"x"},)"
        R"(7,)"
        R"({"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},"severity":4,"message":"h"}]})",
        "ab");
    // 行が本文の外の診断は捨てずに末尾の空の区間（EOF の診断）として残す（ADR 0161 の決定 2）。
    const std::vector<Diagnostic> expected{
        Diagnostic{OffsetRange{Offset{2}, Offset{2}}, DiagnosticSeverity::error, "x"},
        Diagnostic{OffsetRange{Offset{0}, Offset{1}}, DiagnosticSeverity::hint, "h"}};
    expect(kept.has_value() && kept.value().diagnostics == expected,
           "malformed entries and unknown severities are dropped; lines outside clamp to the end");
}

void verify_refused()
{
    constexpr std::array<std::string_view, 7> refused{{
        R"([])",
        R"({"diagnostics":[]})",
        R"({"uri":1,"diagnostics":[]})",
        R"({"uri":"u"})",
        R"({"uri":"u","diagnostics":{}})",
        R"({"uri":"u","version":-1,"diagnostics":[]})",
        R"({"uri":"u","version":"7","diagnostics":[]})",
    }};
    for (const std::string_view params : refused)
    {
        const Published answer = published_of(params, "a");
        expect(!answer.has_value() && answer.error() == LspDiagnosticsFailure::invalid_params,
               "malformed publishDiagnostics params are refused");
    }
    const core::LspNotification other{
        .method = "window/logMessage",
        .params = notification_of(R"({"uri":"u","diagnostics":[]})").params};
    const Published wrong = core::lsp_published_diagnostics(other, text_of("a"), utf16);
    expect(!wrong.has_value() && wrong.error() == LspDiagnosticsFailure::not_publish_diagnostics,
           "another notification is not read as diagnostics");
}

[[nodiscard]] std::string many_diagnostics(std::size_t count)
{
    std::string params = R"({"uri":"u","diagnostics":[)";
    // 上限の数え方: 捨てる 1 件を先頭に置き、残した件だけを数えることを確かめる。
    params +=
        R"({"severity":9,"message":"x","range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}}})";
    for (std::size_t index = 0; index < count; ++index)
    {
        params +=
            R"(,{"message":"x","range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}}})";
    }
    return params + "]}";
}

void verify_limit()
{
    const Published full = published_of(many_diagnostics(core::lsp_diagnostics_limit), "ab");
    expect(full.has_value() && full.value().diagnostics.size() == core::lsp_diagnostics_limit &&
               full.value().count == core::DiagnosticsCount::complete,
           "exactly 1,000 diagnostics are all kept");
    const Published over = published_of(many_diagnostics(core::lsp_diagnostics_limit + 5), "ab");
    expect(over.has_value() && over.value().diagnostics.size() == core::lsp_diagnostics_limit &&
               over.value().count == core::DiagnosticsCount::truncated,
           "diagnostics past 1,000 are dropped and the count is marked truncated");
}

void verify_shifts()
{
    for (const auto &[input, expected] : shifts)
    {
        const auto &[range, edit] = input;
        const Edit applied{Offset{edit.first.first}, std::string(edit.first.second, 'r'),
                           std::string{edit.second}, Offset{edit.first.first}};
        const std::vector<Diagnostic> after = core::diagnostics_adjusted(
            {diagnostic_at(range.first, range.second, DiagnosticSeverity::warning)}, applied);
        const bool same =
            expected.has_value()
                ? after.size() == 1 &&
                      after.front().range == OffsetRange{Offset{expected.value().first},
                                                         Offset{expected.value().second}} &&
                      after.front().severity == DiagnosticSeverity::warning
                : after.empty();
        expect(same, "an edit keeps diagnostics before it, shifts those after it and drops those "
                     "it touches");
    }
    const std::vector<Diagnostic> mixed =
        core::diagnostics_adjusted({diagnostic_at(9, 12, DiagnosticSeverity::error),
                                    diagnostic_at(0, 2, DiagnosticSeverity::hint),
                                    diagnostic_at(4, 6, DiagnosticSeverity::warning)},
                                   Edit{Offset{5}, "", "xx", Offset{5}});
    const std::vector<Diagnostic> expected{diagnostic_at(11, 14, DiagnosticSeverity::error),
                                           diagnostic_at(0, 2, DiagnosticSeverity::hint)};
    expect(mixed == expected, "the surviving diagnostics keep their order");
}

void verify_spans()
{
    constexpr auto error = DiagnosticSeverity::error;
    constexpr auto warning = DiagnosticSeverity::warning;
    constexpr auto hint = DiagnosticSeverity::hint;
    // 2 行目「日本語abc」は本文の 4 バイト目から（1 行目は "xy\r\n"）。日本語は 3 バイトで 1 桁。
    const std::string_view line = "日本語abc";
    const Offset start{4};
    const std::vector<Diagnostic> overlapping{
        diagnostic_at(4, 13, hint), diagnostic_at(7, 14, error), diagnostic_at(13, 15, warning)};
    const std::vector<DiagnosticSpan> heavier{span_of(1, 2, hint), span_of(2, 5, error),
                                              span_of(5, 6, warning)};
    expect(core::diagnostic_spans(line, start, overlapping) == heavier,
           "overlapping diagnostics keep only the heavier one, in ascending columns");
    const std::vector<Diagnostic> crossing{diagnostic_at(0, 7, warning),
                                           diagnostic_at(14, 30, warning)};
    const std::vector<DiagnosticSpan> clipped{span_of(1, 2, warning), span_of(5, 7, warning)};
    expect(core::diagnostic_spans(line, start, crossing) == clipped,
           "diagnostics crossing lines are clipped to the line content");
    const std::vector<Diagnostic> touching{diagnostic_at(4, 7, warning),
                                           diagnostic_at(7, 10, warning)};
    const std::vector<DiagnosticSpan> joined{span_of(1, 3, warning)};
    expect(core::diagnostic_spans(line, start, touching) == joined,
           "adjacent spans of the same severity are joined");
    const std::vector<Diagnostic> empty{diagnostic_at(7, 7, error), diagnostic_at(16, 16, hint),
                                        diagnostic_at(17, 17, hint), diagnostic_at(0, 4, error)};
    const std::vector<DiagnosticSpan> widened{span_of(2, 3, error), span_of(7, 8, hint)};
    expect(core::diagnostic_spans(line, start, empty) == widened,
           "an empty diagnostic in the line becomes one column; others outside are skipped");
}

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return bound == 0 ? 0 : static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] std::string random_text(Random &random, std::size_t length)
{
    constexpr std::string_view letters = "ab \n";
    std::string text;
    for (std::size_t index = 0; index < length; ++index)
    {
        text.push_back(letters[below(random, letters.size())]);
    }
    return text;
}

// 本文の上の昇順の診断の列（重なりも空も作る）。
[[nodiscard]] std::vector<Diagnostic> random_diagnostics(Random &random, std::size_t size)
{
    std::vector<Diagnostic> diagnostics;
    std::size_t begin = 0;
    const std::size_t count = below(random, 8);
    for (std::size_t index = 0; index < count && begin <= size; ++index)
    {
        const std::size_t end = begin + below(random, size - begin + 1);
        diagnostics.push_back(
            diagnostic_at(begin, end, static_cast<DiagnosticSeverity>(below(random, 4))));
        begin += below(random, 4);
    }
    return diagnostics;
}

// 残った診断の本文が編集の前と同じで、本文の中にあり、並びが保たれるか。
[[nodiscard]] bool shifted_well(const std::string &before, const std::string &after,
                                const std::vector<Diagnostic> &original,
                                const std::vector<Diagnostic> &kept)
{
    std::size_t next = 0;
    std::size_t previous_begin = 0;
    for (const Diagnostic &diagnostic : kept)
    {
        while (next < original.size() && original[next].message != diagnostic.message)
        {
            ++next;
        }
        if (next == original.size())
        {
            return false;
        }
        const OffsetRange &was = original[next].range;
        const OffsetRange &is = diagnostic.range;
        const bool inside = is.begin.value <= is.end.value && is.end.value <= after.size();
        const bool ordered = previous_begin <= is.begin.value;
        if (!inside || !ordered ||
            before.substr(was.begin.value, was.end.value - was.begin.value) !=
                after.substr(is.begin.value, is.end.value - is.begin.value))
        {
            return false;
        }
        previous_begin = is.begin.value;
        ++next;
    }
    return true;
}

void verify_shift_property()
{
    Random random{property_seed};
    std::size_t broken = 0;
    for (std::size_t round = 0; round < property_rounds; ++round)
    {
        const std::string before = random_text(random, below(random, 24));
        std::vector<Diagnostic> diagnostics = random_diagnostics(random, before.size());
        // 文を通し番号にして、残った件が元のどれかを並びで追えるようにする。
        for (std::size_t index = 0; index < diagnostics.size(); ++index)
        {
            diagnostics[index].message = std::to_string(index);
        }
        const std::size_t at = below(random, before.size() + 1);
        const std::size_t removed = below(random, before.size() - at + 1);
        const std::string inserted = random_text(random, below(random, 4));
        const Edit edit{Offset{at}, before.substr(at, removed), inserted, Offset{at}};
        const std::string after = before.substr(0, at) + inserted + before.substr(at + removed);
        const std::vector<Diagnostic> kept = core::diagnostics_adjusted(diagnostics, edit);
        broken += static_cast<std::size_t>(!shifted_well(before, after, diagnostics, kept));
    }
    expect(broken == 0, "shifted diagnostics stay inside the text, keep their order and cover the "
                        "same text as before the edit");
}
} // namespace

void verify_lsp_diagnostics_scope()
{
    verify_ranges();
    verify_published();
    verify_dropped_entries();
    verify_refused();
    verify_limit();
    verify_shifts();
    verify_spans();
    verify_shift_property();
}
} // namespace nenenib::tests
