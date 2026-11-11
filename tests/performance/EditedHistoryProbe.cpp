#include "EditedHistoryProbe.hpp"

#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "Milestone.hpp"
#include "OpenDocument.hpp"
#include "SelectEditMode.hpp"
#include "TextViewGeometry.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VisibleLines.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

namespace nenenib::tests::performance
{
namespace
{
// 1 行目の長さ（N に依らない・区間の `x` 200 回の後も文字が残る・巨大な行のしきい値より短い）。
constexpr std::size_t line_characters = 712U;
// 区間で流す鍵の数（`l` `h` の 100 組か `x` の 200 回）。
constexpr std::size_t timed_keys = 200U;

[[nodiscard]] std::size_t edits_of(EditedHistoryWorkload workload) noexcept
{
    switch (workload)
    {
    case EditedHistoryWorkload::moves_after_1000:
    case EditedHistoryWorkload::erases_after_1000:
        return 1000U;
    case EditedHistoryWorkload::moves_after_4000:
    case EditedHistoryWorkload::erases_after_4000:
        return 4000U;
    case EditedHistoryWorkload::moves_after_16000:
    case EditedHistoryWorkload::erases_after_16000:
        return 16000U;
    }
    std::unreachable();
}

[[nodiscard]] bool erases(EditedHistoryWorkload workload) noexcept
{
    switch (workload)
    {
    case EditedHistoryWorkload::moves_after_1000:
    case EditedHistoryWorkload::moves_after_4000:
    case EditedHistoryWorkload::moves_after_16000:
        return false;
    case EditedHistoryWorkload::erases_after_1000:
    case EditedHistoryWorkload::erases_after_4000:
    case EditedHistoryWorkload::erases_after_16000:
        return true;
    }
    std::unreachable();
}

void pressed(application::EditorController &controller, char32_t key)
{
    static_cast<void>(
        controller.apply(application::VimKeyPress{core::VimKey{core::VimCharacter{key}}}));
}
// 区間の外: 文書を開いて Vim にし、1 行目の行頭で `rb` と `rc` を交互に N 回（undo の単位が N 個
// 積まれ、行の長さは変わらない）。
void prepare_history(Editing &editing, EditedHistoryWorkload workload, const std::string &input)
{
    auto &controller = editing.controller();
    editing.files().hold(input);
    static_cast<void>(controller.apply(
        application::VisibleLines{20U, core::TextViewGeometry{800, 20, 96, 10.0F}}));
    static_cast<void>(controller.apply(
        application::OpenDocument{core::FilePath::parse("C:\\nib-probe\\history.txt").value()}));
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::vim}));
    for (std::size_t edit = 0; edit < edits_of(workload); ++edit)
    {
        pressed(controller, U'r');
        pressed(controller, edit % 2U == 0U ? U'b' : U'c');
    }
    static_cast<void>(controller.frame());
}

// 区間の key 番目の鍵（`x` か、`l` と `h` を交互に）。
[[nodiscard]] char32_t timed_key(EditedHistoryWorkload workload, std::size_t key) noexcept
{
    if (erases(workload))
    {
        return U'x';
    }
    return key % 2U == 0U ? U'l' : U'h';
}

// 桁は 1 始まり。`x` は行頭のまま、`l` `h` は 2 と 1 を交互に。
[[nodiscard]] std::uint64_t expected_checksum(EditedHistoryWorkload workload) noexcept
{
    return erases(workload) ? timed_keys : timed_keys / 2U * 3U;
}

// 区間の後（時間の外）: 積んだ undo の単位が units 個あることを `u` で確かめる（レビュー V-1481 の
// L4）。units - 1 回戻すと最初の `rb` だけが残り（行頭は b）、もう 1 回で元の行（行頭は
// a・長さも元） に戻る。単位が畳まれていれば数が合わず、ここで落ちる。
[[nodiscard]] bool holds_units(application::EditorController &controller, std::size_t units)
{
    for (std::size_t undo = 0; undo + 1U < units; ++undo)
    {
        pressed(controller, U'u');
    }
    const bool first_left = controller.frame().lines.at(0).text.starts_with('b');
    pressed(controller, U'u');
    const std::string line = controller.frame().lines.at(0).text;
    return first_left && line.size() == line_characters && line.starts_with('a');
}
} // namespace

std::string edited_history_input(EditedHistoryWorkload /*workload*/)
{
    std::string input(line_characters, 'a');
    input += "\nsecond line\n";
    return input;
}

std::expected<std::uint64_t, ProbeFailure> edited_history_keys(EditedHistoryWorkload workload,
                                                               const std::string &input,
                                                               application::TimingPort &timing)
{
    if (input != edited_history_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    Editing editing;
    auto &controller = editing.controller();
    prepare_history(editing, workload, input);
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t key = 0; key < timed_keys; ++key)
    {
        pressed(controller, timed_key(workload, key));
        checksum += controller.frame().caret.position.column.value;
    }
    timing.mark(core::Milestone::probe_finished);
    const auto frame = controller.frame();
    const std::size_t units = edits_of(workload) + (erases(workload) ? timed_keys : 0U);
    if (frame.caret.position.line.value != 1U || frame.total_lines != 3U ||
        checksum != expected_checksum(workload) || !holds_units(controller, units))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::cerr << "undoUnits=" << units << '\n';
    return checksum + units;
}
} // namespace nenenib::tests::performance
