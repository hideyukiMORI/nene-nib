#include "CommandInputProbe.hpp"

#include "CommandInput.hpp"
#include "CommandInputControllerProbe.hpp"
#include "Milestone.hpp"
#include "Utf8.hpp"

#include <array>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::tests::performance
{
namespace
{
struct CommandInputSample
{
    std::string_view input;
    std::string_view expected;
    std::size_t caret;
    std::size_t code_points;
    std::uint64_t checksum;
    std::size_t alternative;
};

constexpr std::array<CommandInputSample, 12> cases{{
    {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa日🖋",
     247U, 242U, 505856U, 2U},
    {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa日🖋",
     247U, 242U, 505856U, 0U},
    {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa日🖋",
     247U, 242U, 505856U, 1U},
    {"a", "a日🖋", 8U, 3U, 16384U, 2U},
    {"a", "a日🖋", 8U, 3U, 16384U, 0U},
    {"a", "a日🖋", 8U, 3U, 16384U, 1U},
    {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa🖋",
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa🖋",
     240U, 241U, 495616U, 0U},
    {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
     ":aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
     241U, 241U, 493568U, 1U},
    {"a", "a", 1U, 1U, 1152U, 2U},
    {"a", "a", 1U, 1U, 1152U, 0U},
    {"a", "a", 1U, 1U, 1152U, 1U},
    {":fz", ":fz", 3U, 3U, 2304U, 1U},
}};

[[nodiscard]] const CommandInputSample &sample_of(CommandInputWorkload workload)
{
    return cases.at(static_cast<std::size_t>(workload));
}

[[nodiscard]] bool matches(const core::CommandLine &line, std::string_view text, std::size_t caret)
{
    return line.text() == text && line.caret().value == caret && line.completions().empty() &&
           !line.completion_index().has_value();
}

[[nodiscard]] bool matches(const core::SearchLine &line, std::string_view text, std::size_t caret)
{
    return line.text() == text && line.caret().value == caret &&
           line.direction() == core::VimSearchDirection::forward;
}

[[nodiscard]] bool matches(const core::CommandPalette &palette, std::string_view text,
                           std::size_t caret)
{
    const auto scope =
        text.starts_with(":") ? core::PaletteScope::commands : core::PaletteScope::files;
    return matches(palette.input(), text, caret) && palette.scope() == scope &&
           palette.count() == 0U && palette.selected() == 0U && palette.rows(0U, 1U).empty();
}

[[nodiscard]] bool input_equal(const application::CommandInput &input, std::string_view text,
                               std::size_t caret, std::size_t alternative)
{
    return input.index() == alternative &&
           std::visit([text, caret](const auto &line) { return matches(line, text, caret); },
                      input);
}

[[nodiscard]] bool view_equal(const core::InputLineView &view, std::string_view text,
                              std::size_t caret, std::size_t alternative)
{
    constexpr std::array prompts{core::InputLinePrompt::ex, core::InputLinePrompt::palette,
                                 core::InputLinePrompt::search_forward};
    return view.text == text && view.caret.value == caret &&
           view.prompt == prompts.at(alternative) && view.completions.empty() &&
           !view.completion_index.has_value();
}

[[nodiscard]] application::CommandInput opened_input(CommandInputWorkload workload,
                                                     const CommandInputSample &sample)
{
    switch (workload)
    {
    case CommandInputWorkload::search_long:
    case CommandInputWorkload::search_short:
        return core::SearchLine::opened(core::VimSearchDirection::forward)
            .inserted(sample.input)
            .value();
    case CommandInputWorkload::ex_long:
    case CommandInputWorkload::ex_short:
        return core::CommandLine::empty().inserted(sample.input).value();
    case CommandInputWorkload::palette_long:
    case CommandInputWorkload::palette_short:
        return core::CommandPalette::opened({}, sample.input, core::EditMode::ordinary);
    case CommandInputWorkload::ex_left_long:
    case CommandInputWorkload::palette_fill_long:
    case CommandInputWorkload::controller_search:
    case CommandInputWorkload::controller_ex:
    case CommandInputWorkload::controller_palette:
    case CommandInputWorkload::controller_fill:
        break;
    }
    std::unreachable();
}

[[nodiscard]] std::uint64_t input_checksum(const core::SearchLine &line)
{
    return line.text().size() + line.caret().value;
}

[[nodiscard]] std::uint64_t input_checksum(const core::CommandLine &line)
{
    return line.text().size() + line.caret().value;
}

[[nodiscard]] std::uint64_t input_checksum(const core::CommandPalette &palette)
{
    return input_checksum(palette.input());
}

[[nodiscard]] std::uint64_t input_checksum(const application::CommandInput &input)
{
    return std::visit([](const auto &line) { return input_checksum(line); }, input);
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
inserted_probe(CommandInputWorkload workload, const CommandInputSample &sample,
               application::TimingPort &timing)
{
    const auto source = opened_input(workload, sample);
    const auto source_view = application::input_line_of(source);
    const auto initial = application::inserted_command(source, "日🖋").value();
    const auto held_view = application::input_line_of(initial);
    if (!input_equal(source, sample.input, sample.input.size(), sample.alternative) ||
        !input_equal(initial, sample.expected, sample.caret, sample.alternative) ||
        !view_equal(held_view, sample.expected, sample.caret, sample.alternative) ||
        core::code_point_count(held_view.text) != sample.code_points)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < 1024U; ++index)
    {
        const auto next = application::inserted_command(source, "日🖋");
        checksum += input_checksum(next.value());
    }
    timing.mark(core::Milestone::probe_finished);
    if (checksum != sample.checksum ||
        !input_equal(source, sample.input, sample.input.size(), sample.alternative) ||
        !view_equal(source_view, sample.input, sample.input.size(), sample.alternative) ||
        !input_equal(initial, sample.expected, sample.caret, sample.alternative) ||
        !view_equal(held_view, sample.expected, sample.caret, sample.alternative) ||
        !input_equal(application::inserted_command(source, "日🖋").value(), sample.expected,
                     sample.caret, sample.alternative))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
left_probe(const CommandInputSample &sample, application::TimingPort &timing)
{
    const auto source = core::CommandLine::empty().inserted(sample.input).value();
    const std::string_view source_view = source.text();
    const auto initial = source.edited(core::CommandEdit::left);
    const std::string_view held_view = initial.text();
    if (!matches(source, sample.input, sample.input.size()) ||
        !matches(initial, sample.expected, sample.caret) ||
        core::code_point_count(held_view) != sample.code_points)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < 1024U; ++index)
    {
        const auto next = source.edited(core::CommandEdit::left);
        checksum += input_checksum(next);
    }
    timing.mark(core::Milestone::probe_finished);
    if (checksum != sample.checksum || !matches(source, sample.input, sample.input.size()) ||
        source_view != sample.input || !matches(initial, sample.expected, sample.caret) ||
        held_view != sample.expected ||
        !matches(source.edited(core::CommandEdit::left), sample.expected, sample.caret))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
filled_probe(const CommandInputSample &sample, application::TimingPort &timing)
{
    const auto source = core::CommandPalette::opened({}, "old", core::EditMode::ordinary);
    const std::string_view source_view = source.input().text();
    const auto initial = source.filled(sample.input).value();
    const std::string_view held_view = initial.input().text();
    if (!matches(source, "old", 3U) || !matches(initial, sample.expected, sample.caret) ||
        core::code_point_count(held_view) != sample.code_points)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < 1024U; ++index)
    {
        const auto next = source.filled(sample.input);
        checksum += input_checksum(next.value());
    }
    timing.mark(core::Milestone::probe_finished);
    if (checksum != sample.checksum || !matches(source, "old", 3U) || source_view != "old" ||
        !matches(initial, sample.expected, sample.caret) || held_view != sample.expected ||
        !matches(source.filled(sample.input).value(), sample.expected, sample.caret))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace

std::string command_input_input(CommandInputWorkload workload)
{
    return std::string(sample_of(workload).input);
}

std::expected<std::uint64_t, ProbeFailure> probed_command_input(CommandInputWorkload workload,
                                                                const std::string &input,
                                                                application::TimingPort &timing)
{
    if (input != command_input_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto &sample = sample_of(workload);
    switch (workload)
    {
    case CommandInputWorkload::search_long:
    case CommandInputWorkload::ex_long:
    case CommandInputWorkload::palette_long:
    case CommandInputWorkload::search_short:
    case CommandInputWorkload::ex_short:
    case CommandInputWorkload::palette_short:
        return inserted_probe(workload, sample, timing);
    case CommandInputWorkload::ex_left_long:
        return left_probe(sample, timing);
    case CommandInputWorkload::palette_fill_long:
        return filled_probe(sample, timing);
    case CommandInputWorkload::controller_search:
    case CommandInputWorkload::controller_ex:
    case CommandInputWorkload::controller_palette:
    case CommandInputWorkload::controller_fill:
        return probed_controller_input(workload, input, timing);
    }
    std::unreachable();
}
} // namespace nenenib::tests::performance
