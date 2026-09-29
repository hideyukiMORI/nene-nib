#include "EditorState.hpp"

#include "SaveState.hpp"
#include "SearchLine.hpp"
#include "TabTitle.hpp"
#include "VimStep.hpp"

#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace nenenib::application
{
namespace
{
// 新規の本文は空・無題・UTF-8 で、改行は CRLF（ADR 0009 の決定 8）。位置 0 が保存時点なので、
// 何も打たずに閉じるときは未保存にならない（ADR 0010 の決定 7）。
constexpr std::size_t first_line = 1;
constexpr std::size_t initial_visible_lines = 1;

// 起動のときの文書と NewTab の文書は同じ「空の無題」の 1 つ（ADR 0056 の決定 3・ARC-001）。
[[nodiscard]] DocumentState untitled_document()
{
    const Document document{std::nullopt, core::TextEncoding::utf8, std::size_t{0}};
    return DocumentState{core::TextBuffer::empty(),
                         core::collapsed_at(core::Offset{0}),
                         core::EditHistory::empty(),
                         document,
                         core::LineNumber{first_line},
                         std::nullopt,
                         std::nullopt,
                         DocumentView{core::tab_title_for(std::nullopt, core::SaveState::saved),
                                      std::nullopt, document.encoding, core::SaveState::saved,
                                      std::nullopt}};
}

[[nodiscard]] std::ptrdiff_t distance_of(std::size_t index) noexcept
{
    return static_cast<std::ptrdiff_t>(index);
}
} // namespace

EditorState::EditorState(core::Appearance appearance, core::EditMode mode, DocumentState untitled)
    : text_(std::move(untitled.text)), selection_(untitled.selection),
      history_(std::move(untitled.history)),
      scroll_(ScrollState{untitled.first_visible, initial_visible_lines}), appearance_(appearance),
      mode_(mode), vim_(core::vim_resting_state(
                       core::VimRegister{std::string{}, core::VimRegisterKind::uninitialized})),
      document_(std::move(untitled.document)), settings_(core::default_editor_settings())
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
    return EditorState(appearance, mode, untitled_document());
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
    return text_.line_ending();
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
    // preview は検索の入力行が開いているあいだだけある（ADR 0041 の決定 2）。どの経路で入力行が
    // 閉じても（Enter・Esc・モードの切替・Ex への置き換え）ここで一緒に消える。
    if (!command.has_value() || !std::holds_alternative<core::SearchLine>(command.value()))
    {
        next.search_preview_ = std::nullopt;
    }
    next.command_input_ = std::move(command);
    return next;
}

const std::optional<SearchPreview> &EditorState::search_preview() const noexcept
{
    return search_preview_;
}

EditorState EditorState::with_search_preview(std::optional<SearchPreview> preview) const
{
    EditorState next(*this);
    next.search_preview_ = preview;
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

EditorState EditorState::with_opened(core::TextBuffer text, Document document) const
{
    EditorState next(*this);
    next.text_ = std::move(text);
    next.selection_ = core::collapsed_at(core::Offset{0});
    next.history_ = core::EditHistory::empty();
    next.scroll_ = ScrollState{core::LineNumber{first_line}, scroll_.visible_lines};
    next.document_ = std::move(document);
    return next;
}

std::size_t EditorState::tab_count() const noexcept
{
    return parked_.size() + 1;
}

std::size_t EditorState::active_tab() const noexcept
{
    return active_;
}

const std::vector<std::shared_ptr<const DocumentState>> &EditorState::parked() const noexcept
{
    return parked_;
}

bool EditorState::closing() const noexcept
{
    return closing_;
}

EditorState EditorState::with_closing(bool closing) const
{
    EditorState next(*this);
    next.closing_ = closing;
    return next;
}

std::int32_t EditorState::title_bar_width() const noexcept
{
    return title_bar_width_;
}

std::int32_t EditorState::tab_scroll() const noexcept
{
    return tab_scroll_;
}

const std::optional<core::TitleBarTarget> &EditorState::hovered() const noexcept
{
    return hovered_;
}

EditorState EditorState::with_title_bar_width(std::int32_t dip) const
{
    EditorState next(*this);
    next.title_bar_width_ = dip;
    return next;
}

EditorState EditorState::with_tab_scroll(std::int32_t dip) const
{
    EditorState next(*this);
    next.tab_scroll_ = dip;
    return next;
}

EditorState EditorState::with_hovered(std::optional<core::TitleBarTarget> hovered) const
{
    EditorState next(*this);
    next.hovered_ = hovered;
    return next;
}

// 置く（ADR 0056 の決定 1・4）。出ていく文書の選択はキャレットへ畳み（VISUAL を持ち越さない）、
// undo の単位を閉じる。題名は置くこの 1 回だけ作り、frame は並べるだけにする。
std::shared_ptr<const DocumentState> EditorState::parked_active() const
{
    auto history = history_.sealed();
    const auto save_state = save_state_of(document_, history.position());
    DocumentView view{core::tab_title_for(document_.path, save_state), document_.path,
                      document_.encoding, save_state, std::nullopt};
    return std::make_shared<const DocumentState>(DocumentState{
        text_, core::collapsed_at(selection_.caret), std::move(history), document_,
        scroll_.first_visible, vim_.wanted_column, vim_.scroll_lines, std::move(view)});
}

// 広げる（決定 1・4）。見えている行数は窓全体の値なので保ち、先頭行だけを文書から取る。
// Vim の文書ごとの値（欲しい列と 'scroll'）は engine の純関数が入れ替える。
void EditorState::spread(const DocumentState &tab)
{
    text_ = tab.text;
    selection_ = tab.selection;
    history_ = tab.history.sealed();
    document_ = tab.document;
    scroll_ = ScrollState{tab.first_visible, scroll_.visible_lines};
    vim_ = core::vim_switched_document(vim_, tab.wanted_column, tab.scroll_lines);
}

EditorState EditorState::with_switched(std::size_t position) const
{
    if (position >= tab_count() || position == active_)
    {
        return *this;
    }
    EditorState next(*this);
    // 今の文書を元の位置に置くと、脇の束の添字が帯の位置と一致する。
    next.parked_.insert(std::next(next.parked_.begin(), distance_of(active_)), parked_active());
    const auto target = next.parked_.at(position);
    next.parked_.erase(std::next(next.parked_.begin(), distance_of(position)));
    next.active_ = position;
    next.spread(*target);
    return next;
}

EditorState EditorState::with_new_tab() const
{
    EditorState next(*this);
    next.parked_.insert(std::next(next.parked_.begin(), distance_of(active_)), parked_active());
    next.active_ = active_ + 1;
    next.spread(untitled_document());
    return next;
}

EditorState EditorState::with_closed(std::size_t position) const
{
    if (position >= tab_count() || tab_count() == 1)
    {
        return *this;
    }
    EditorState next(*this);
    if (position != active_)
    {
        // 脇の束は帯の位置からアクティブを抜いた順なので、右側は 1 つ左へずれる。
        const std::size_t index = position < active_ ? position : position - 1;
        next.parked_.erase(std::next(next.parked_.begin(), distance_of(index)));
        next.active_ = position < active_ ? active_ - 1 : active_;
        return next;
    }
    // 右隣は脇の束の添字 active_、左隣は active_ - 1。どちらも閉じた後の帯の位置と一致する。
    const std::size_t index = active_ < parked_.size() ? active_ : active_ - 1;
    const auto target = next.parked_.at(index);
    next.parked_.erase(std::next(next.parked_.begin(), distance_of(index)));
    next.active_ = index;
    next.spread(*target);
    return next;
}
} // namespace nenenib::application
