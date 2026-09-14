#include "EditorState.hpp"

#include <utility>

namespace nenenib::application
{
namespace
{
// 新規の本文は空で、改行は CRLF（ADR 0009 の決定 8）。読み込みが入るまではここが唯一の初期値。
constexpr std::size_t first_line = 1;
constexpr std::size_t initial_visible_lines = 1;
} // namespace

EditorState::EditorState(core::Appearance appearance, core::EditMode mode)
    : text_(core::TextBuffer::empty()), selection_(core::collapsed_at(core::Offset{0})),
      history_(core::EditHistory::empty()),
      scroll_(ScrollState{core::LineNumber{first_line}, initial_visible_lines}),
      line_ending_(core::LineEnding::crlf), appearance_(appearance), mode_(mode)
{
}

EditorState EditorState::create(core::Appearance appearance, core::EditMode mode)
{
    return EditorState(appearance, mode);
}

const core::TextBuffer &EditorState::text() const noexcept
{
    return text_;
}

const core::Selection &EditorState::selection() const noexcept
{
    return selection_;
}

const core::EditHistory &EditorState::history() const noexcept
{
    return history_;
}

const ScrollState &EditorState::scroll() const noexcept
{
    return scroll_;
}

core::LineEnding EditorState::line_ending() const noexcept
{
    return line_ending_;
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
    EditorState next(*this);
    next.appearance_ = appearance;
    return next;
}

EditorState EditorState::with_mode(core::EditMode mode) const
{
    EditorState next(*this);
    next.mode_ = mode;
    return next;
}

EditorState EditorState::with_selection(const core::Selection &selection) const
{
    EditorState next(*this);
    next.selection_ = selection;
    return next;
}

EditorState EditorState::with_scroll(const ScrollState &scroll) const
{
    EditorState next(*this);
    next.scroll_ = scroll;
    return next;
}

EditorState EditorState::with_edit(core::TextBuffer text, const core::Selection &selection,
                                   core::EditHistory history) const
{
    EditorState next(*this);
    next.text_ = std::move(text);
    next.selection_ = selection;
    next.history_ = std::move(history);
    return next;
}
} // namespace nenenib::application
