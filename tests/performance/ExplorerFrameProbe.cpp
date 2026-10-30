#include "ExplorerFrameProbe.hpp"

#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "ExplorerView.hpp"
#include "FilePath.hpp"
#include "FolderBatch.hpp"
#include "FolderEntry.hpp"
#include "FolderProgress.hpp"
#include "InsertText.hpp"
#include "Milestone.hpp"
#include "ProbeWorkload.hpp"
#include "ScriptedFolders.hpp"
#include "ScriptedSettings.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace nenenib::tests::performance
{
namespace
{
// 面のフォルダの件数・本文の見えている行数（面の行数はルート名を除いた 60）・打鍵の数。
constexpr std::size_t entry_count = 1000;
constexpr std::size_t body_lines = 61;
constexpr std::size_t panel_rows = 60;
constexpr std::size_t keystrokes = 64;

[[nodiscard]] bool shown(ExplorerFrameWorkload workload) noexcept
{
    switch (workload)
    {
    case ExplorerFrameWorkload::typing_shown:
        return true;
    case ExplorerFrameWorkload::typing_hidden:
        return false;
    }
    std::unreachable();
}

[[nodiscard]] SettingsReading settings_of(bool panel)
{
    auto settings = core::default_editor_settings();
    settings.explorer = panel;
    settings.root = core::FilePath::parse("C:\\proj").value();
    return settings;
}

// 面を出したときは、最後の要求の券で 1,000 件を返して合図を送る（区間の外）。
[[nodiscard]] bool listed(Editing &editing)
{
    if (editing.folders().requests().empty())
    {
        return false;
    }
    application::FolderBatch batch{
        editing.folders().requests().back().ticket, {}, application::FolderProgress::complete};
    for (std::size_t index = 0; index < entry_count; ++index)
    {
        batch.entries.push_back(application::FolderEntry{
            core::FilePath::parse(std::format("C:\\proj\\file-{:04}.txt", index)).value(), false,
            false});
    }
    editing.folders().serve(std::move(batch));
    const auto frame = editing.controller().apply_frame(application::WorkCompleted{});
    return frame.explorer.has_value() && frame.explorer.value().total == entry_count;
}

[[nodiscard]] std::size_t rows_of(const application::EditorFrame &frame)
{
    return frame.explorer.has_value() ? frame.explorer.value().rows.size() : 0U;
}
} // namespace

std::string explorer_frame_input(ExplorerFrameWorkload workload)
{
    return std::format("explorer-frame-v1\n{}\n{}\n{}\n{}\n", shown(workload) ? "shown" : "hidden",
                       entry_count, body_lines, keystrokes);
}

std::expected<std::uint64_t, ProbeFailure> explorer_frame_typing(ExplorerFrameWorkload workload,
                                                                 const std::string &input,
                                                                 application::TimingPort &timing)
{
    if (input != explorer_frame_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const bool panel = shown(workload);
    Editing editing(settings_of(panel));
    static_cast<void>(editing.controller().apply(application::VisibleLines{body_lines}));
    if (panel && !listed(editing))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const application::InsertText intent{"a"};
    std::uint64_t checksum = 0;
    std::optional<application::EditorFrame> last;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < keystrokes; ++index)
    {
        last = editing.controller().apply_frame(intent);
        checksum += rows_of(last.value()) + 1U;
    }
    timing.mark(core::Milestone::probe_finished);
    const std::size_t expected_rows = panel ? panel_rows : 0U;
    if (!last.has_value() || rows_of(last.value()) != expected_rows ||
        last.value().caret.position.column.value != keystrokes + 1 ||
        checksum != keystrokes * (expected_rows + 1U))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum + checksum_of(input);
}
} // namespace nenenib::tests::performance
