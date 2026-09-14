#include "EditorController.hpp"

#include "Palette.hpp"

#include <utility>

namespace nenenib::application
{
namespace
{
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
      state_(EditorState::create(std::move(text), appearance_or_dark(appearance)))
{
}

EditorFrame EditorController::apply(EditorIntent intent)
{
    switch (intent)
    {
    case EditorIntent::refresh_appearance:
        state_ = state_.with_appearance(appearance_or_dark(appearance_));
        return frame();
    }
    std::unreachable();
}

EditorFrame EditorController::frame() const
{
    return EditorFrame{state_.text(), core::palette_for(state_.appearance())};
}
} // namespace nenenib::application
