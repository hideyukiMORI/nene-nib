#include "EditorController.hpp"

#include "EditMode.hpp"
#include "ModeLabel.hpp"
#include "Palette.hpp"
#include "StatusItems.hpp"

#include <cstddef>
#include <utility>

namespace nenenib::application
{
namespace
{
// 編集もファイルも無い間の固定値。キャレットの位置とタブの題名は、
// 編集とタブの縦切りで EditorState が持つ（ADR 0008 の「正直に記録しておくこと」）。
constexpr std::size_t first_line = 1;
constexpr std::size_t first_column = 1;
constexpr char untitled[] = "無題";

// 読めない理由（unavailable / unreadable）は区別せず既定の dark を選ぶ。
// 区別が要る判断が生まれたら、そのときに EditorFrame へ載せる（ADR 0007）。
[[nodiscard]] core::Appearance appearance_or_dark(const AppearancePort &port)
{
    const auto current = port.current();
    if (!current)
    {
        return core::Appearance::dark;
    }
    return *current;
}
} // namespace

EditorController::EditorController(const AppearancePort &appearance, core::DisplayText text)
    : appearance_(appearance),
      state_(EditorState::create(std::move(text), appearance_or_dark(appearance),
                                 core::EditMode::ordinary))
{
}

EditorFrame EditorController::apply(EditorIntent intent)
{
    switch (intent)
    {
    case EditorIntent::refresh_appearance:
        state_ = state_.with_appearance(appearance_or_dark(appearance_));
        return frame();
    case EditorIntent::select_ordinary_mode:
        state_ = state_.with_mode(core::EditMode::ordinary);
        return frame();
    case EditorIntent::select_vim_mode:
        state_ = state_.with_mode(core::EditMode::vim);
        return frame();
    }
    std::unreachable();
}

EditorFrame EditorController::frame() const
{
    return EditorFrame{state_.text(),
                       state_.appearance(),
                       core::palette_for(state_.appearance()),
                       state_.mode(),
                       core::mode_label(state_.mode()),
                       core::DisplayText::parse(untitled).value(),
                       core::status_items_for(first_line, first_column)};
}
} // namespace nenenib::application
