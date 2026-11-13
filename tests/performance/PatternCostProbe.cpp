#include "PatternCostProbe.hpp"

#include "Milestone.hpp"
#include "ProbeWorkload.hpp"
#include "VimPattern.hpp"
#include "VimPatternCase.hpp"
#include "VimPatternRoute.hpp"
#include "VimSearchDirection.hpp"

#include <array>
#include <cstddef>
#include <iostream>
#include <string_view>
#include <vector>

namespace nenenib::tests::performance
{
namespace
{
// 1 つの本文の形にある workload の数（lead 4・tail 4・quantity 1）。
constexpr std::size_t forms_per_text = 9U;
constexpr std::array<std::size_t, 4> star_counts{16U, 64U, 256U, 1024U};
constexpr std::size_t long_line_chars = 16000U;
constexpr std::size_t short_lines = 2000U;
constexpr std::size_t short_line_chars = 40U;

[[nodiscard]] std::size_t form_of(PatternCostWorkload workload)
{
    return static_cast<std::size_t>(workload) % forms_per_text;
}

[[nodiscard]] bool many_lines(PatternCostWorkload workload)
{
    return static_cast<std::size_t>(workload) >= forms_per_text;
}

[[nodiscard]] std::string pattern_cost_text(PatternCostWorkload workload)
{
    const std::size_t form = form_of(workload);
    if (form == 2U * star_counts.size())
    {
        return "a\\{,1000}c";
    }
    std::string stars;
    for (std::size_t star = 0; star < star_counts.at(form % star_counts.size()); ++star)
    {
        stars += "a*";
    }
    return form < star_counts.size() ? "z" + stars : stars + "c";
}

[[nodiscard]] std::vector<std::string_view> lines_of(std::string_view text)
{
    std::vector<std::string_view> lines;
    std::size_t begin = 0;
    while (true)
    {
        const std::size_t end = text.find('\n', begin);
        if (end == std::string_view::npos)
        {
            lines.push_back(text.substr(begin));
            return lines;
        }
        lines.push_back(text.substr(begin, end - begin));
        begin = end + 1U;
    }
}
} // namespace

std::string pattern_cost_input(PatternCostWorkload workload)
{
    if (!many_lines(workload))
    {
        return std::string(long_line_chars, 'a');
    }
    std::string text;
    for (std::size_t line = 0; line < short_lines; ++line)
    {
        text.append(short_line_chars, 'a');
        if (line + 1U < short_lines)
        {
            text += '\n';
        }
    }
    return text;
}

std::expected<std::uint64_t, ProbeFailure> pattern_cost_run(PatternCostWorkload workload,
                                                            const std::string &input,
                                                            application::TimingPort &timing)
{
    const std::string source = pattern_cost_text(workload);
    const auto pattern = core::VimPattern::parse(source, core::VimSearchDirection::forward,
                                                 core::VimPatternCase::sensitive);
    if (!pattern.has_value() || pattern.value().route() == core::VimPatternRoute::sequence ||
        input != pattern_cost_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const std::vector<std::string_view> lines = lines_of(input);
    std::size_t matches = 0;
    timing.mark(core::Milestone::probe_started);
    for (const std::string_view line : lines)
    {
        matches += pattern.value().matched(line, 0U).has_value() ? 1U : 0U;
    }
    timing.mark(core::Milestone::probe_finished);
    if (matches != 0U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::cerr << "atoms=" << pattern.value().atom_count() << " lines=" << lines.size() << '\n';
    return checksum_of(input) + pattern.value().atom_count() + lines.size();
}
} // namespace nenenib::tests::performance
