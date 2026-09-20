#include "EditorState.hpp"

#include <optional>
#include <string>
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
      vim_(core::vim_resting_state(
          core::VimRegister{std::string{}, core::VimRegisterKind::uninitialized})),
      document_(Document{std::nullopt, core::TextEncoding::utf8, std::size_t{0}}),
      settings_(core::default_editor_settings())
{
}

const core::ThemeCatalog &EditorState::themes() const noexcept
{
    return themes_;
}

EditorState EditorState::with_themes(core::ThemeCatalog themes) const
{
    EditorState next = *this;
    next.themes_ = std::move(themes);
    return next;
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

const core::VimState &EditorState::vim() const noexcept
{
    return vim_;
}

const Document &EditorState::document() const noexcept
{
    return document_;
}

const std::optional<core::Composition> &EditorState::composition() const noexcept
{
    return composition_;
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

const core::EditorSettings &EditorState::settings() const noexcept
{
    return settings_;
}

std::optional<SettingsIssue> EditorState::settings_failure() const noexcept
{
    return settings_failure_;
}

EditorState EditorState::with_settings(core::EditorSettings settings) const
{
    EditorState next(*this);
    next.settings_ = std::move(settings);
    return next;
}

EditorState EditorState::with_settings_failure(std::optional<SettingsIssue> failure) const
{
    EditorState next(*this);
    next.settings_failure_ = failure;
    return next;
}

EditorState EditorState::with_mode(core::EditMode mode) const
{
    EditorState next(*this);
    next.mode_ = mode;
    return next;
}

const std::optional<CommandInput> &EditorState::command_input() const noexcept
{
    return command_input_;
}

const std::optional<core::DisplayText> &EditorState::command_message() const noexcept
{
    return command_message_;
}

EditorState EditorState::with_command_input(std::optional<CommandInput> command) const
{
    EditorState next(*this);
    next.command_input_ = std::move(command);
    return next;
}

EditorState EditorState::with_command_message(std::optional<core::DisplayText> message) const
{
    EditorState next(*this);
    next.command_message_ = std::move(message);
    return next;
}

EditorState EditorState::with_vim(core::VimState vim) const
{
    EditorState next(*this);
    next.vim_ = std::move(vim);
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

EditorState EditorState::with_composition(std::optional<core::Composition> composition) const
{
    EditorState next(*this);
    next.composition_ = std::move(composition);
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
