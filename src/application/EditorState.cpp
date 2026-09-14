#include "EditorState.hpp"

#include <utility>

namespace nenenib::application
{
EditorState::EditorState(core::DisplayText text, core::Appearance appearance)
    : text_(std::move(text)), appearance_(appearance)
{
}

EditorState EditorState::create(core::DisplayText text, core::Appearance appearance)
{
    return EditorState(std::move(text), appearance);
}

const core::DisplayText &EditorState::text() const noexcept
{
    return text_;
}

core::Appearance EditorState::appearance() const noexcept
{
    return appearance_;
}

EditorState EditorState::with_appearance(core::Appearance appearance) const
{
    return EditorState(text_, appearance);
}
} // namespace nenenib::application
