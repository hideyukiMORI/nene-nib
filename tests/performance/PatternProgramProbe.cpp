#include "PatternProgramProbe.hpp"

#include "Milestone.hpp"
#include "ProbeWorkload.hpp"
#include "VimPattern.hpp"
#include "VimPatternRoute.hpp"

#include <array>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::tests::performance
{
namespace
{
constexpr std::size_t probe_bytes = 4096;

[[nodiscard]] std::string_view program_text(PatternProgramWorkload workload)
{
    switch (workload)
    {
    case PatternProgramWorkload::branch_star_miss_4096:
        return "\\(a\\|b\\)*c";
    case PatternProgramWorkload::group_bounded_miss_4096:
        return "\\(ab\\|a\\)\\{,50}c";
    }
    std::unreachable();
}
} // namespace

std::string pattern_program_input(PatternProgramWorkload /*workload*/)
{
    return std::string(probe_bytes, 'a');
}

// 区間は照合 16 回（pattern- の 7 本と同じ回数）。どれも当たらない。
std::expected<std::uint64_t, ProbeFailure> matched_program(PatternProgramWorkload workload,
                                                           const std::string &input,
                                                           application::TimingPort &timing)
{
    const std::string_view source = program_text(workload);
    const auto pattern = core::VimPattern::parse(source, core::VimSearchDirection::forward,
                                                 core::VimPatternCase::sensitive);
    if (!pattern.has_value() || pattern.value().route() != core::VimPatternRoute::program ||
        input != pattern_program_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::array<std::optional<core::VimPatternMatch>, 16> observed{};
    std::cerr << "patternHashAlgorithm=fnv1a64 patternHash=" << checksum_of(source)
              << " instructions=" << pattern.value().instruction_count() << '\n';
    timing.mark(core::Milestone::probe_started);
    for (auto &result : observed)
    {
        result = pattern.value().matched(input, 0U);
    }
    timing.mark(core::Milestone::probe_finished);
    for (const auto &result : observed)
    {
        if (result.has_value())
        {
            return std::unexpected(ProbeFailure::wrong_result);
        }
    }
    return checksum_of(input) + pattern.value().instruction_count();
}
} // namespace nenenib::tests::performance
