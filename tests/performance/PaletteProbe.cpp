#include "PaletteProbe.hpp"

#include "CommandPalette.hpp"
#include "Milestone.hpp"
#include "ProbePaletteInput.hpp"

#include <format>

namespace nenenib::tests::performance
{
namespace
{
[[nodiscard]] std::string narrow_label(std::size_t index)
{
    if (index < 50U)
    {
        return std::format("QX_Y_{:05d}_file.txt", index);
    }
    if (index < 500U)
    {
        return std::format("QX_N_{:05d}_file.txt", index);
    }
    return std::format("AA_N_{:05d}_file.txt", index);
}

[[nodiscard]] std::string palette_input(std::string_view query, std::string_view detail)
{
    std::string input = std::format("files\t{}\n", query);
    for (std::size_t index = 0; index < 5000U; ++index)
    {
        const std::string label =
            detail.empty() ? narrow_label(index)
                           : std::format("ENTRY_{:05d}_ABCDEFGHIJKLMNOPQRSTUV.txt", index);
        input += std::format("folder\topen\tprobe-{:05d}\t{}\t{}\n", index, label, detail);
    }
    return input;
}

[[nodiscard]] std::vector<std::string_view> fields_of(std::string_view row)
{
    std::vector<std::string_view> result;
    std::size_t start = 0;
    for (std::size_t at = 0; at < row.size(); ++at)
    {
        if (row[at] == '\t')
        {
            result.push_back(row.substr(start, at - start));
            start = at + 1U;
        }
    }
    result.push_back(row.substr(start));
    return result;
}

[[nodiscard]] std::expected<core::CommandChoice, ProbeFailure> choice_of(std::string_view row)
{
    const auto fields = fields_of(row);
    if (fields.size() != 5U || fields[0] != "folder" || fields[1] != "open")
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto label = core::DisplayText::parse(fields[3]);
    if (!label.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::optional<core::DisplayText> detail;
    if (!fields[4].empty())
    {
        const auto parsed = core::DisplayText::parse(fields[4]);
        if (!parsed.has_value())
        {
            return std::unexpected(ProbeFailure::wrong_result);
        }
        detail = parsed.value();
    }
    return core::CommandChoice{label.value(),
                               std::string(fields[2]),
                               core::CommandChoiceKind::open,
                               std::move(detail),
                               core::PaletteOrigin::folder,
                               std::nullopt,
                               ""};
}

[[nodiscard]] std::expected<ProbePaletteInput, ProbeFailure> palette_of(std::string_view input)
{
    const auto header_end = input.find('\n');
    const auto header = fields_of(input.substr(0, header_end));
    if (header.size() != 2U || header[0] != "files" || header_end == std::string_view::npos)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    ProbePaletteInput result{std::string(header[1]), {}};
    result.entries.reserve(5000U);
    std::size_t start = header_end + 1U;
    while (start < input.size())
    {
        const auto end = input.find('\n', start);
        if (end == std::string_view::npos)
        {
            return std::unexpected(ProbeFailure::wrong_result);
        }
        auto choice = choice_of(input.substr(start, end - start));
        if (!choice.has_value())
        {
            return std::unexpected(choice.error());
        }
        result.entries.push_back(std::move(choice.value()));
        start = end + 1U;
    }
    if (result.entries.size() != 5000U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return result;
}

[[nodiscard]] bool ordered_positions(const std::vector<std::size_t> &positions)
{
    if (positions.size() != 5000U)
    {
        return false;
    }
    for (std::size_t index = 0; index < positions.size(); ++index)
    {
        if (positions[index] != index)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool palette_matches(const core::CommandPalette &palette, std::size_t count,
                                   std::string_view query, std::size_t caret)
{
    if (palette.count() != count || palette.input().text() != query ||
        palette.input().caret().value != caret || palette.scope() != core::PaletteScope::files)
    {
        return false;
    }
    const auto rows = palette.rows(0U, count);
    if (rows.size() != count)
    {
        return false;
    }
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto &row = rows[index];
        if (row.command != std::format("probe-{:05d}", index) ||
            row.label.text() != narrow_label(index) || row.detail.has_value() ||
            row.origin != core::PaletteOrigin::folder || row.kind != core::CommandChoiceKind::open)
        {
            return false;
        }
    }
    return true;
}
} // namespace

std::string named_palette_input()
{
    return palette_input("entry", "D:\\ZROOT\\LONG_DIRECTORY_COMPONENT\\GROUP_00");
}

std::string located_palette_input()
{
    return palette_input("zroot", "D:\\ZROOT\\LONG_DIRECTORY_COMPONENT\\GROUP_00");
}

std::string narrowing_palette_input()
{
    return palette_input("qx", "");
}

std::expected<std::uint64_t, ProbeFailure> listed_palette(const std::string &input,
                                                          application::TimingPort &timing)
{
    const auto prepared = palette_of(input);
    if (!prepared.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::vector<std::size_t> positions;
    timing.mark(core::Milestone::probe_started);
    positions = core::listed_positions(prepared.value().entries, core::PaletteScope::files,
                                       prepared.value().query);
    timing.mark(core::Milestone::probe_finished);
    if (!ordered_positions(positions))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(input) ^ positions.size();
}

std::expected<std::uint64_t, ProbeFailure> narrowed_palette(const std::string &input,
                                                            application::TimingPort &timing)
{
    auto prepared = palette_of(input);
    if (!prepared.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto palette = core::CommandPalette::opened(
        std::move(prepared.value().entries), prepared.value().query, core::EditMode::ordinary);
    if (!palette_matches(palette, 500U, "qx", 2U) || palette.selected() != 0U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::optional<std::expected<core::CommandPalette, core::ExFailure>> observed;
    timing.mark(core::Milestone::probe_started);
    observed = palette.inserted("y");
    timing.mark(core::Milestone::probe_finished);
    if (!observed.has_value() || !observed.value().has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto &result = observed.value().value();
    if (!palette_matches(result, 50U, "qxy", 3U) || result.selected() != 0U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(result.input().text()) ^ result.count();
}

std::expected<std::uint64_t, ProbeFailure> left_palette(const std::string &input,
                                                        application::TimingPort &timing)
{
    auto prepared = palette_of(input);
    if (!prepared.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto palette =
        core::CommandPalette::opened(std::move(prepared.value().entries), prepared.value().query,
                                     core::EditMode::ordinary)
            .selected_at(7U);
    if (!palette_matches(palette, 500U, "qx", 2U) || palette.selected() != 7U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::optional<core::CommandPalette> observed;
    timing.mark(core::Milestone::probe_started);
    observed = palette.edited(core::CommandEdit::left);
    timing.mark(core::Milestone::probe_finished);
    if (!observed.has_value() || !palette_matches(observed.value(), 500U, "qx", 1U) ||
        observed.value().selected() != 0U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(observed.value().input().text()) ^ observed.value().count() ^ 1U;
}
} // namespace nenenib::tests::performance
