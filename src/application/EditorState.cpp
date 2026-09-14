#include "EditorState.hpp"

#include <utility>

namespace nenenib::application
{
EditorState::EditorState(core::DisplayText text, core::Appearance appearance, core::EditMode mode)
    : text_(std::move(text)), appearance_(appearance), mode_(mode)
{
}

EditorState EditorState::create(core::DisplayText text, core::Appearance appearance,
                                core::EditMode mode)
{
    return EditorState(std::move(text), appearance, mode);
}

const core::DisplayText &EditorState::text() const noexcept
{
    return text_;
}

core::Appearance EditorState::appearance() const noexcept
{
    return appearance_;
}

core::EditMode EditorState::mode() const noexcept
{
    return mode_;
}

EditorState EditorState::with_appearance(core::Appearance appearance) const
{
    return EditorState(text_, appearance, mode_);
}

EditorState EditorState::with_mode(core::EditMode mode) const
{
    return EditorState(text_, appearance_, mode);
}
} // namespace nenenib::application
