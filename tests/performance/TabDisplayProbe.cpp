#include "TabDisplayProbe.hpp"

#include "DisplayText.hpp"
#include "FilePath.hpp"
#include "Milestone.hpp"
#include "SaveState.hpp"
#include "TabTitle.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace nenenib::tests::performance
{
namespace
{
struct TabDisplayCase
{
    std::string_view prefix;
    std::string_view segment;
    std::size_t repetitions;
    std::string_view suffix;
    core::SaveState state;
    std::string_view title;
    std::size_t title_code_points;
    std::string_view folder;
    std::size_t folder_code_points;
    std::uint64_t checksum;
};

constexpr std::array<TabDisplayCase, 9> cases{{
    {"C:\\Users\\hide\\Documents\\NeNeNib\\projects\\editor\\", "a", 64U, ".txt",
     core::SaveState::saved, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.txt",
     68U, "C:\\Users\\hide\\Documents\\NeNeNib\\projects\\editor", 47U, 236544U},
    {"C:\\", "segment\\", 128U, "note.txt", core::SaveState::saved, "note.txt", 8U,
     "C:"
     "\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\s"
     "egment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segm"
     "ent\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment"
     "\\se…",
     254U, 539648U},
    {"C:\\work\\", "日本語", 64U, ".txt", core::SaveState::saved,
     "日本語日本語日本語日本語日本語日本語日本語日本語日本語日本語日本語日本語日本語日本語日本語日"
     "本語日本語日本語日本語日本語日本語日本語日本語日本語日本語日本語日本語日本語…",
     85U, "C:\\work", 7U, 363520U},
    {"C:\\n.txt", "", 0U, "", core::SaveState::saved, "n.txt", 5U, "C:", 2U, 15360U},
    {"", "a", 256U, "", core::SaveState::saved,
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
     256U, "", 0U, 524288U},
    {"", "a", 247U, "🖋.txt", core::SaveState::modified,
     "● "
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa…",
     250U, "", 0U, 516096U},
    {"C:/", "資料/", 64U, "note.txt", core::SaveState::saved, "note.txt", 8U,
     "C:/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/"
     "資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資料/資…",
     110U, 390144U},
    {"C:\\", "", 0U, "", core::SaveState::saved, "C:\\", 3U, "C:", 2U, 11264U},
    {"", "", 0U, "", core::SaveState::modified, "● 無題", 4U, "", 0U, 14336U},
}};

[[nodiscard]] const TabDisplayCase &tab_display_case(TabDisplayWorkload workload)
{
    return cases.at(static_cast<std::size_t>(workload));
}

[[nodiscard]] bool display_equal(const core::DisplayText &title,
                                 const std::optional<core::DisplayText> &folder,
                                 const TabDisplayCase &sample)
{
    const bool location = folder.has_value() == !sample.folder.empty();
    return title.text() == sample.title && title.code_point_count() == sample.title_code_points &&
           location &&
           (!folder.has_value() ||
            (folder.value().text() == sample.folder &&
             folder.value().code_point_count() == sample.folder_code_points));
}

[[nodiscard]] bool path_equal(const std::optional<core::FilePath> &path, std::string_view input)
{
    return path.has_value() ? path.value().text() == input : input.empty();
}

[[nodiscard]] std::uint64_t display_checksum(const core::DisplayText &title,
                                             const std::optional<core::DisplayText> &folder)
{
    const std::size_t location =
        folder.has_value() ? folder.value().text().size() + folder.value().code_point_count() + 1U
                           : 0U;
    return title.text().size() + title.code_point_count() + location;
}
} // namespace

std::string tab_display_input(TabDisplayWorkload workload)
{
    const auto &sample = tab_display_case(workload);
    std::string input(sample.prefix);
    for (std::size_t index = 0; index < sample.repetitions; ++index)
    {
        input += sample.segment;
    }
    return input + std::string(sample.suffix);
}

std::expected<std::uint64_t, ProbeFailure> fixed_tab_display(TabDisplayWorkload workload,
                                                             const std::string &input,
                                                             application::TimingPort &timing)
{
    const auto &sample = tab_display_case(workload);
    if (input != tab_display_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::optional<core::FilePath> path;
    if (!input.empty())
    {
        const auto parsed = core::FilePath::parse(input);
        if (!parsed)
        {
            return std::unexpected(ProbeFailure::wrong_result);
        }
        path = parsed.value();
    }
    const auto initial_title = core::tab_title_for(path, sample.state);
    const auto initial_folder = core::tab_folder_for(path);
    if (!path_equal(path, input) || !display_equal(initial_title, initial_folder, sample))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t iteration = 0; iteration < 1024U; ++iteration)
    {
        const auto title = core::tab_title_for(path, sample.state);
        const auto folder = core::tab_folder_for(path);
        checksum += display_checksum(title, folder);
    }
    timing.mark(core::Milestone::probe_finished);
    if (checksum != sample.checksum || !path_equal(path, input) ||
        !display_equal(initial_title, initial_folder, sample) ||
        !display_equal(core::tab_title_for(path, sample.state), core::tab_folder_for(path), sample))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace nenenib::tests::performance
