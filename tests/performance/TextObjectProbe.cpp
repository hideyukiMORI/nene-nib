#include "TextObjectProbe.hpp"

#include "GiantLine.hpp"
#include "Milestone.hpp"
#include "ProbeWorkload.hpp"
#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimMode.hpp"
#include "VimRegisterKind.hpp"
#include "VimTextObject.hpp"
#include "VimTextObjectOutcome.hpp"
#include "VimTextObjectRange.hpp"
#include "VimTextObjectRequest.hpp"
#include "VimTextObjectScope.hpp"

#include <array>
#include <cstddef>
#include <iostream>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::tests::performance
{
namespace
{
// 期待の範囲は Vim 9.1 で同じ本文に `y{オブジェクト}` を打って測った値（#732・`'[` と無名
// レジスタの長さ）。閉じの無い `at` は取り消しで、キャレットは動かない。
// 区間で引く回数は軽い 2 本が 16 回、重い 2 本（巨大な行の `is`・閉じの無い `at`）は 1 回。
// 重い 2 本は Debug の 1 回が秒の単位で、16 回では probe-smoke が数分かかる（#732 の初回）。
constexpr std::size_t dense_bytes = 4096U;
constexpr std::size_t tag_depth = 200U;
constexpr std::size_t unclosed_opens = 500U;
constexpr std::array<std::size_t, 4> carets{8192U, 2048U, 3U, 4095U};
constexpr std::array<core::VimTextObjectScope, 4> scopes{
    core::VimTextObjectScope::inner, core::VimTextObjectScope::around,
    core::VimTextObjectScope::inner, core::VimTextObjectScope::around};
constexpr std::array<core::VimTextObject, 4> objects{
    core::VimTextObject::sentence, core::VimTextObject::sentence, core::VimTextObject::tag,
    core::VimTextObject::tag};
constexpr std::array<std::size_t, 4> rounds{1U, 16U, 16U, 1U};
constexpr std::array<bool, 4> spanned{true, true, true, false};
constexpr std::array<std::size_t, 4> begins{0U, 2000U, 3U, 0U};
constexpr std::array<std::size_t, 4> ends{core::giant_line_threshold, 2050U, 4092U, 0U};

[[nodiscard]] std::string cycled_words(std::size_t length)
{
    constexpr std::string_view words = "lorem ipsum dolor sit amet ";
    std::string text;
    text.reserve(length);
    while (text.size() < length)
    {
        text += words.at(text.size() % words.size());
    }
    return text;
}

[[nodiscard]] bool outcome_equals(const core::VimTextObjectOutcome &outcome, std::size_t index)
{
    if (const auto *span = std::get_if<core::VimTextObjectSpan>(&outcome); span != nullptr)
    {
        return spanned.at(index) && span->range.kind == core::VimRegisterKind::characters &&
               span->range.range.begin.value == begins.at(index) &&
               span->range.range.end.value == ends.at(index);
    }
    const auto &cancel = std::get<core::VimTextObjectCancel>(outcome);
    return !spanned.at(index) &&
           cancel.selection == core::collapsed_at(core::Offset{carets.at(index)});
}

[[nodiscard]] std::uint64_t outcome_sum(const core::VimTextObjectOutcome &outcome)
{
    if (const auto *span = std::get_if<core::VimTextObjectSpan>(&outcome); span != nullptr)
    {
        return 1U + span->range.range.begin.value + span->range.range.end.value;
    }
    return std::get<core::VimTextObjectCancel>(outcome).selection.caret.value;
}
// 巨大な行のしきい値ちょうどの 1 行。`.` `!` `?` を含まず、最後は空白でない文字。
[[nodiscard]] std::string giant_sentence_text()
{
    std::string text = cycled_words(core::giant_line_threshold);
    text.back() = 'z';
    return text;
}

// 48 字の語の並びと ". " の 50 字を繰り返し、4,096 字で切る（最後の文はピリオド無し）。
[[nodiscard]] std::string dense_sentence_text()
{
    const std::string unit = cycled_words(48U) + ". ";
    std::string text;
    text.reserve(dense_bytes + unit.size());
    while (text.size() < dense_bytes)
    {
        text += unit;
    }
    text.resize(dense_bytes);
    return text;
}

// 段ごとに `<a>y` を 200 回、中身の x、閉じ `</a>` を 200 回。全体で 4,096 字。
[[nodiscard]] std::string nested_tag_text()
{
    std::string text;
    for (std::size_t level = 0; level < tag_depth; ++level)
    {
        text += "<a>y";
    }
    text.append(dense_bytes - (tag_depth * 8U), 'x');
    for (std::size_t level = 0; level < tag_depth; ++level)
    {
        text += "</a>";
    }
    return text;
}

// 閉じの無い `<a>` を 500 個、その後ろに x。全体で 4,096 字。
[[nodiscard]] std::string unclosed_tag_text()
{
    std::string text;
    for (std::size_t open = 0; open < unclosed_opens; ++open)
    {
        text += "<a>";
    }
    text.append(dense_bytes - (unclosed_opens * 3U), 'x');
    return text;
}
} // namespace

std::string text_object_input(TextObjectWorkload workload)
{
    switch (workload)
    {
    case TextObjectWorkload::sentence_giant:
        return giant_sentence_text();
    case TextObjectWorkload::sentence_dense:
        return dense_sentence_text();
    case TextObjectWorkload::tag_nested:
        return nested_tag_text();
    case TextObjectWorkload::tag_unclosed:
        return unclosed_tag_text();
    }
    std::unreachable();
}

std::expected<std::uint64_t, ProbeFailure> text_object_ranges(TextObjectWorkload workload,
                                                              const std::string &input,
                                                              application::TimingPort &timing)
{
    const auto text = core::TextBuffer::from_utf8(input);
    const auto index = static_cast<std::size_t>(workload);
    if (!text.has_value() || input != text_object_input(workload) ||
        text.value().line_count() != 1U || text.value().piece_count() != 1U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const core::Selection selection = core::collapsed_at(core::Offset{carets.at(index)});
    const core::VimTextObjectRequest request{scopes.at(index), objects.at(index),
                                             core::VimMode::normal};
    std::array<core::VimTextObjectOutcome, 16> observed{};
    std::cerr << "preparedPieces=1 preparedHash=" << checksum_of(input) << '\n';
    timing.mark(core::Milestone::probe_started);
    for (std::size_t round = 0; round < rounds.at(index); ++round)
    {
        observed.at(round) = core::vim_text_object_range(text.value(), selection, request, 1U);
    }
    timing.mark(core::Milestone::probe_finished);
    std::uint64_t checksum = checksum_of(input);
    for (std::size_t round = 0; round < rounds.at(index); ++round)
    {
        const auto &outcome = observed.at(round);
        if (!outcome_equals(outcome, index))
        {
            return std::unexpected(ProbeFailure::wrong_result);
        }
        checksum += outcome_sum(outcome);
    }
    return checksum;
}
} // namespace nenenib::tests::performance
