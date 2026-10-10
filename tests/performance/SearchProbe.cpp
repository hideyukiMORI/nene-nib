#include "SearchProbe.hpp"

#include "Milestone.hpp"
#include "TextBuffer.hpp"
#include "VimPattern.hpp"
#include "VimSearch.hpp"

#include <array>
#include <iostream>

namespace nenenib::tests::performance
{
std::string search_buffer_input()
{
    std::string input;
    input.reserve(61440U);
    for (std::size_t index = 0; index < 4096U; ++index)
    {
        input += "a日本語🖋 ";
    }
    return input;
}

std::expected<std::uint64_t, ProbeFailure>
searched_buffer(ProbeWorkload workload, const std::string &input, application::TimingPort &timing)
{
    const auto text = core::TextBuffer::from_utf8(input);
    const auto pattern = core::VimPattern::parse("a", core::VimSearchDirection::forward);
    if (!text.has_value() || !pattern.has_value() || text.value().text() != search_buffer_input() ||
        text.value().piece_count() != 1U || text.value().line_count() != 1U ||
        text.value().size_bytes() != 61440U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    constexpr std::array<std::size_t, 3> columns{1U, 12289U, 24571U};
    constexpr std::array<std::size_t, 6> destinations{7U, 12295U, 1U, 24571U, 12283U, 24565U};
    constexpr std::array<bool, 6> wraps{false, false, true, true, false, false};
    const std::size_t index = static_cast<std::size_t>(workload) -
                              static_cast<std::size_t>(ProbeWorkload::search_forward_head);
    const core::VimMatchRequest request{
        {core::LineNumber{1U}, core::Column{columns.at(index % 3U)}},
        index < 3U ? core::VimSearchDirection::forward : core::VimSearchDirection::backward,
        1U};
    std::array<std::expected<core::VimSearchHit, core::VimSearchNoticeKind>, 16> observed{};
    std::cerr << "preparedPieces=1 preparedHash=" << checksum_of(input) << '\n';
    timing.mark(core::Milestone::probe_started);
    for (auto &hit : observed)
    {
        hit = core::vim_find_match(text.value(), pattern.value(), request);
    }
    timing.mark(core::Milestone::probe_finished);
    std::uint64_t checksum = checksum_of(input);
    for (const auto &hit : observed)
    {
        if (!hit.has_value() || hit.value().position.line.value != 1U ||
            hit.value().position.column.value != destinations.at(index) ||
            hit.value().wrapped != wraps.at(index))
        {
            return std::unexpected(ProbeFailure::wrong_result);
        }
        checksum += hit.value().position.line.value + hit.value().position.column.value +
                    static_cast<std::uint64_t>(hit.value().wrapped);
    }
    if (text.value().text() != input)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace nenenib::tests::performance
