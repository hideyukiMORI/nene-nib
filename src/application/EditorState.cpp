#include "EditorState.hpp"

#include <optional>
#include <utility>

namespace nenenib::application
{
namespace
{
// 新規の本文は空・無題・UTF-8 で、改行は CRLF（ADR 0009 の決定 8）。位置 0 が保存時点なので、
// 何も打たずに閉じるときは未保存にならない（ADR 0010 の決定 7）。
constexpr std::size_t first_line = 1;
constexpr std::size_t initial_visible_lines = 1;
} // namespace

EditorState::EditorState(core::Appearance appearance, core::EditMode mode)
    : text_(core::TextBuffer::empty()), selection_(core::collapsed_at(core::Offset{0})),
      history_(core::EditHistory::empty()),
      scroll_(ScrollState{core::LineNumber{first_line}, initial_visible_lines}),
      line_ending_(core::LineEnding::crlf), appearance_(appearance), mode_(mode),
      document_(Document{std::nullopt, core::TextEncoding::utf8, std::size_t{0}})
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

const Document &EditorState::document() const noexcept
{
    return document_;
}

std::optional<FileFailure> EditorState::last_failure() const noexcept
{
    return last_failure_;
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

EditorState EditorState::with_history(core::EditHistory history) const
{
    EditorState next(*this);
    next.history_ = std::move(history);
    return next;
}

EditorState EditorState::with_document(Document document) const
{
    EditorState next(*this);
    next.document_ = std::move(document);
    return next;
}

EditorState EditorState::with_failure(std::optional<FileFailure> failure) const
{
    EditorState next(*this);
    next.last_failure_ = failure;
    return next;
}

EditorState EditorState::with_opened(core::TextBuffer text, core::LineEnding ending,
                                     Document document) const
{
    EditorState next(*this);
    next.text_ = std::move(text);
    next.selection_ = core::collapsed_at(core::Offset{0});
    next.history_ = core::EditHistory::empty();
    next.scroll_ = ScrollState{core::LineNumber{first_line}, scroll_.visible_lines};
    next.line_ending_ = ending;
    next.document_ = std::move(document);
    return next;
}
} // namespace nenenib::application
