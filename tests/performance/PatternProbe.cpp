#include "PatternProbe.hpp"

#include "Milestone.hpp"
#include "VimPattern.hpp"

#include <array>
#include <iostream>

namespace nenenib::tests::performance
{
namespace
{
[[nodiscard]] std::size_t pattern_index(ProbeWorkload workload)
{
    return static_cast<std::size_t>(workload) -
           static_cast<std::size_t>(ProbeWorkload::pattern_star_small);
}

[[nodiscard]] std::string pattern_text(ProbeWorkload workload)
{
    constexpr std::array<std::string_view, 6> patterns{"a*b",       "a*b",  "a*b",
                                                       "a*a*a*a*b", "a.*a", "needle"};
    if (workload == ProbeWorkload::pattern_literal_long)
    {
        return std::string(4096U, 'a');
    }
    return std::string(patterns.at(pattern_index(workload)));
}
[[nodiscard]] bool matched_equals(const std::optional<core::VimPatternMatch> &result,
                                  std::size_t index)
{
    constexpr std::array<bool, 7> present{false, false, false, false, true, true, true};
    constexpr std::array<std::size_t, 7> begins{0U, 0U, 0U, 0U, 0U, 4096U, 0U};
    constexpr std::array<std::size_t, 7> ends{0U, 0U, 0U, 0U, 4096U, 4102U, 4096U};
    if (result.has_value() != present.at(index))
    {
        return false;
    }
    if (!result.has_value())
    {
        return true;
    }
    return result.value().begin == begins.at(index) && result.value().end == ends.at(index);
}
} // namespace

std::string pattern_probe_input(ProbeWorkload workload)
{
    constexpr std::array<std::size_t, 7> sizes{1024U, 2048U, 4096U, 32U, 4096U, 4096U, 4096U};
    if (workload == ProbeWorkload::pattern_literal_tail)
    {
        return std::string(4096U, 'x') + "needle";
    }
    return std::string(sizes.at(pattern_index(workload)), 'a');
}

std::expected<std::uint64_t, ProbeFailure>
matched_pattern(ProbeWorkload workload, const std::string &input, application::TimingPort &timing)
{
    const std::string source = pattern_text(workload);
    const auto pattern = core::VimPattern::parse(source, core::VimSearchDirection::forward);
    if (!pattern.has_value() || input != pattern_probe_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const std::size_t index = pattern_index(workload);
    std::array<std::optional<core::VimPatternMatch>, 16> observed{};
    std::cerr << "patternHashAlgorithm=fnv1a64 patternHash=" << checksum_of(source) << '\n';
    timing.mark(core::Milestone::probe_started);
    for (auto &result : observed)
    {
        result = pattern.value().matched(input, 0U);
    }
    timing.mark(core::Milestone::probe_finished);
    std::uint64_t checksum = checksum_of(input);
    for (const auto &result : observed)
    {
        if (!matched_equals(result, index))
        {
            return std::unexpected(ProbeFailure::wrong_result);
        }
        if (result.has_value())
        {
            checksum += 1U + result.value().begin + result.value().end;
        }
    }
    return checksum;
}
} // namespace nenenib::tests::performance
