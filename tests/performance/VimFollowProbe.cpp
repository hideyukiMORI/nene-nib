#include "VimFollowProbe.hpp"

#include "CommandText.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "Milestone.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "SelectEditMode.hpp"
#include "SubmitCommand.hpp"
#include "TextViewGeometry.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimSpecialKey.hpp"
#include "VisibleLines.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

namespace nenenib::tests::performance
{
namespace
{
// 120 行・各行 30 字（20 桁の折り返す窓では 2 画面行）。窓は 20 行。
constexpr std::size_t follow_lines = 120U;
constexpr std::size_t follow_height = 20U;
constexpr std::int32_t follow_width = 200; // 20 桁（1 升 10 px）
// 区間で打つ鍵の数（<CR> と 1 文字の組を 100 回）。
constexpr std::size_t follow_pairs = 100U;

void pressed(application::EditorController &controller, core::VimKey key)
{
    static_cast<void>(controller.apply(application::VimKeyPress{key}));
}

[[nodiscard]] bool wrapped(VimBlockWorkload workload) noexcept
{
    return workload == VimBlockWorkload::insert_scroll_wrapped;
}

// 文書の末尾の行の終わりで INSERT に入った窓（窓の最後の行にキャレットがある）。
void prepare_follow(Editing &editing, VimBlockWorkload workload, const std::string &input)
{
    auto &controller = editing.controller();
    editing.files().hold(input);
    static_cast<void>(controller.apply(application::VisibleLines{
        follow_height, core::TextViewGeometry{follow_width, 20, 96, 10.0F}}));
    static_cast<void>(controller.apply(
        application::OpenDocument{core::FilePath::parse("C:\\nib-probe\\follow.txt").value()}));
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::vim}));
    if (!wrapped(workload))
    {
        static_cast<void>(controller.apply(application::OpenCommandPalette{}));
        static_cast<void>(controller.apply(application::CommandText{":set nowrap"}));
        static_cast<void>(controller.apply(application::SubmitCommand{}));
    }
    pressed(controller, core::VimKey{core::VimCharacter{U'G'}});
    pressed(controller, core::VimKey{core::VimCharacter{U'A'}});
    static_cast<void>(controller.frame());
}
} // namespace

std::string vim_follow_input()
{
    std::string input;
    for (std::size_t line = 1; line <= follow_lines; ++line)
    {
        std::string text = "line " + std::to_string(line) + " ";
        text.resize(30U, 'w');
        input += text;
        input += line == follow_lines ? "" : "\n";
    }
    return input;
}

// 文書の末尾で Vim の INSERT の `<CR>` と 1 文字を打ち続け、窓が 1 行ずつ送られる形（#1300
// の確かめの 費用・レビュー V-1429 の (3)）。鍵ごとに frame
// を読む（対話と同じに描くたびに追従を解く）。 区間は 200 打鍵。文字組みの問いの回数を
// preparedRequests / layoutRequests に出す。
std::expected<std::uint64_t, ProbeFailure> vim_insert_scrolled(VimBlockWorkload workload,
                                                               const std::string &input,
                                                               application::TimingPort &timing)
{
    if (input != vim_follow_input())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    Editing editing;
    prepare_follow(editing, workload, input);
    auto &controller = editing.controller();
    if (controller.vim_state().mode != core::VimMode::insert)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const std::size_t before = editing.text_layouts().requests();
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t pair = 0; pair < follow_pairs; ++pair)
    {
        pressed(controller, core::VimKey{core::VimSpecialKey::enter});
        checksum += controller.frame().first_visible.value;
        pressed(controller, core::VimKey{core::VimCharacter{U'x'}});
        checksum += controller.frame().first_visible.value;
    }
    timing.mark(core::Milestone::probe_finished);
    const auto frame = controller.frame();
    if (frame.total_lines != follow_lines + follow_pairs ||
        frame.caret.position.line.value != follow_lines + follow_pairs)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::cerr << "layoutRequests=" << (editing.text_layouts().requests() - before)
              << " firstVisible=" << frame.first_visible.value << '\n';
    return checksum;
}
} // namespace nenenib::tests::performance
