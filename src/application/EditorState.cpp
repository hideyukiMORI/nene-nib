#include "EditorState.hpp"

#include "EditAdjust.hpp"
#include "SaveState.hpp"
#include "SearchLine.hpp"
#include "TabTitle.hpp"
#include "VimStep.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

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

// 脇のタブの 2 つの形（ADR 0059 の決定 4）。写し先が足りなければ std::visit が落ちる（CPP-002）。
[[nodiscard]] std::optional<std::shared_ptr<const DocumentState>>
loaded_of(const std::shared_ptr<const DocumentState> &tab)
{
    return tab;
}

[[nodiscard]] std::optional<std::shared_ptr<const DocumentState>>
loaded_of(const std::shared_ptr<const UnloadedDocument> &)
{
    return std::nullopt;
}

[[nodiscard]] std::optional<std::shared_ptr<const DocumentState>>
loaded_bundle(const ParkedTab &tab)
{
    return std::visit([](const auto &value) { return loaded_of(value); }, tab);
}

[[nodiscard]] std::optional<UnloadedDocument>
unloaded_of(const std::shared_ptr<const DocumentState> &)
{
    return std::nullopt;
}

[[nodiscard]] std::optional<UnloadedDocument>
unloaded_of(const std::shared_ptr<const UnloadedDocument> &tab)
{
    return *tab;
}
// 退避番号 id のタブの帯の位置。帯に無ければ値なし。
[[nodiscard]] std::optional<std::size_t> position_of(const std::vector<AutosaveId> &ids,
                                                     AutosaveId id)
{
    const auto found = std::ranges::find(ids, id);
    if (found == ids.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(ids.begin(), found));
}

// まだ動かしていない帯の位置のうち、順位がいちばん大きいもの。同じ順位なら帯の右のもの（帯の順で
// 左ほど最近になる）。まだ動かしていない位置が 1 つは残っているときだけ呼ぶ。
[[nodiscard]] std::size_t oldest_untouched(std::span<const std::size_t> ranks,
                                           const std::vector<bool> &touched)
{
    std::size_t oldest = ranks.size();
    for (std::size_t position = 0; position < ranks.size(); ++position)
    {
        if (!touched.at(position) && (oldest == ranks.size() || ranks[position] >= ranks[oldest]))
        {
            oldest = position;
        }
    }
    return oldest;
}

// 2 枠の向かいの側（枠を移ると操作中の枠の側が反転する・ADR 0148 の決定 2）。
[[nodiscard]] core::PaneSide opposite_side(core::PaneSide side) noexcept
{
    switch (side)
    {
    case core::PaneSide::first:
        return core::PaneSide::second;
    case core::PaneSide::second:
        return core::PaneSide::first;
    }
    std::unreachable();
}

// もう 1 つの枠の和型を読む 1 対（ADR 0155 の決定 1）。選択肢が増えたら std::visit が落ちる。
[[nodiscard]] const PaneView *pane_in(const PaneView &view) noexcept
{
    return &view;
}

[[nodiscard]] const PaneView *pane_in(const PreviewView &) noexcept
{
    return nullptr;
}

[[nodiscard]] const PreviewView *preview_in(const PaneView &) noexcept
{
    return nullptr;
}

[[nodiscard]] const PreviewView *preview_in(const PreviewView &view) noexcept
{
    return &view;
}

// 受け身の本文の枠なら、その表示。無い・プレビューなら nullptr。
[[nodiscard]] const PaneView *pane_of(const std::optional<OtherView> &other) noexcept
{
    if (!other.has_value())
    {
        return nullptr;
    }
    return std::visit([](const auto &view) { return pane_in(view); }, other.value());
}

// プレビューの枠なら、その値。無い・本文の枠なら nullptr。
[[nodiscard]] const PreviewView *preview_of(const std::optional<OtherView> &other) noexcept
{
    if (!other.has_value())
    {
        return nullptr;
    }
    return std::visit([](const auto &view) { return preview_in(view); }, other.value());
}

} // namespace

EditorState::EditorState(core::Appearance appearance, core::EditMode mode, DocumentState untitled)
    : text_(std::move(untitled.text)), selection_(untitled.selection),
      history_(std::move(untitled.history)),
      scroll_(ScrollState{untitled.first_visible, initial_visible_lines}), appearance_(appearance),
      mode_(mode), vim_(core::vim_resting_state(
                       core::VimRegister{std::string{}, core::VimRegisterKind::uninitialized})),
      document_(std::move(untitled.document)), vim_marks_(untitled.marks),
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

core::CaretAffinity EditorState::caret_affinity() const noexcept
{
    return caret_affinity_;
}

EditorState EditorState::with_caret_affinity(core::CaretAffinity affinity) const &
{
    return EditorState(*this).with_caret_affinity(affinity);
}

EditorState EditorState::with_caret_affinity(core::CaretAffinity affinity) &&
{
    caret_affinity_ = affinity;
    return std::move(*this);
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

const core::InputHistory &EditorState::ex_history() const noexcept
{
    return ex_history_;
}

const core::InputHistory &EditorState::search_history() const noexcept
{
    return search_history_;
}

EditorState EditorState::with_ex_history(core::InputHistory history) const
{
    EditorState next(*this);
    next.ex_history_ = std::move(history);
    return next;
}

EditorState EditorState::with_search_history(core::InputHistory history) const
{
    EditorState next(*this);
    next.search_history_ = std::move(history);
    return next;
}

const ExplorerState &EditorState::explorer() const noexcept
{
    return *explorer_;
}

EditorState EditorState::with_explorer(ExplorerState explorer) const
{
    EditorState next(*this);
    next.explorer_ = std::make_shared<const ExplorerState>(std::move(explorer));
    return next;
}

EditorState EditorState::with_command_message(std::optional<core::DisplayText> message) const
{
    EditorState next(*this);
    next.command_message_ = std::move(message);
    return next;
}

const std::vector<DocumentReading> &EditorState::readings() const noexcept
{
    return readings_;
}

EditorState EditorState::with_readings(std::vector<DocumentReading> readings) const
{
    EditorState next(*this);
    next.readings_ = std::move(readings);
    return next;
}

EditorState EditorState::with_vim(core::VimState vim) const &
{
    return EditorState(*this).with_vim(std::move(vim));
}

EditorState EditorState::with_vim(core::VimState vim) &&
{
    vim_ = std::move(vim);
    return std::move(*this);
}

EditorState EditorState::with_selection(core::Selection selection) const &
{
    return EditorState(*this).with_selection(selection);
}

EditorState EditorState::with_selection(core::Selection selection) &&
{
    selection_ = selection;
    caret_affinity_ = core::CaretAffinity::leading;
    return std::move(*this);
}

EditorState EditorState::with_scroll(ScrollState scroll) const &
{
    return EditorState(*this).with_scroll(scroll);
}

EditorState EditorState::with_scroll(ScrollState scroll) &&
{
    scroll_ = scroll;
    return std::move(*this);
}

EditorState EditorState::with_edit(core::TextBuffer text, core::Selection selection,
                                   core::EditHistory history, Document document) const &
{
    return EditorState(*this).with_edit(std::move(text), selection, std::move(history),
                                        std::move(document));
}

EditorState EditorState::with_edit(core::TextBuffer text, core::Selection selection,
                                   core::EditHistory history, Document document) &&
{
    text_ = std::move(text);
    selection_ = selection;
    caret_affinity_ = core::CaretAffinity::leading;
    history_ = std::move(history);
    document_ = std::move(document);
    return std::move(*this);
}

EditorState EditorState::with_history(core::EditHistory history) const &
{
    return EditorState(*this).with_history(std::move(history));
}

EditorState EditorState::with_history(core::EditHistory history) &&
{
    history_ = std::move(history);
    return std::move(*this);
}

EditorState EditorState::with_document(Document document) const &
{
    return EditorState(*this).with_document(std::move(document));
}

EditorState EditorState::with_document(Document document) &&
{
    // 名前が変わる保存は言語をパスから引き直す。同じパスのままなら `:set filetype=` の上書きを
    // 保つ（ADR 0154 の決定 5）。
    if (document.path != document_.path)
    {
        language_ = language_of(document);
    }
    document_ = std::move(document);
    return std::move(*this);
}

EditorState EditorState::with_intent_cleared() const
{
    EditorState next(*this);
    next.last_failure_ = std::nullopt;
    next.closing_ = false;
    next.close_request_ = std::nullopt;
    next.operation_request_ = std::nullopt;
    next.display_question_ = false;
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
    next.caret_affinity_ = core::CaretAffinity::leading;
    next.history_ = core::EditHistory::empty();
    next.scroll_ = ScrollState{core::LineNumber{first_line}, scroll_.visible_lines};
    next.document_ = std::move(document);
    next.display_ = core::DisplayForm::text;
    next.language_ = language_of(next.document_);
    next.syntax_enabled_ = true;
    next.display_inquiry_ = DisplayInquiry::unjudged;
    // 本文が丸ごと替わるので、同じ文書を見るもう 1 つの枠は先頭に戻す（ADR 0148 の決定 5）。
    next.restart_other_showing(autosave_ids_.at(active_));
    next.vim_marks_ = core::VimMarks{};
    return next;
}

core::DisplayForm EditorState::display() const noexcept
{
    return display_;
}

EditorState EditorState::with_display(core::DisplayForm display) const
{
    EditorState next(*this);
    next.display_ = display;
    return next;
}

core::Language EditorState::language() const noexcept
{
    return language_;
}

EditorState EditorState::with_language(core::Language language) const
{
    EditorState next(*this);
    next.language_ = language;
    return next;
}

bool EditorState::syntax_enabled() const noexcept
{
    return syntax_enabled_;
}

EditorState EditorState::with_syntax_enabled(bool enabled) const
{
    EditorState next(*this);
    next.syntax_enabled_ = enabled;
    return next;
}

const core::VimMarks &EditorState::vim_marks() const noexcept
{
    return vim_marks_;
}

EditorState EditorState::with_vim_marks(core::VimMarks marks) const
{
    EditorState next(*this);
    next.vim_marks_ = std::move(marks);
    return next;
}

DisplayInquiry EditorState::display_inquiry() const noexcept
{
    return display_inquiry_;
}

EditorState EditorState::with_display_inquiry(DisplayInquiry inquiry) const
{
    EditorState next(*this);
    next.display_inquiry_ = inquiry;
    return next;
}

EditorState EditorState::with_virtual_columns() const
{
    EditorState next(*this);
    next.text_ = text_.with_virtual_columns();
    return next;
}

bool EditorState::display_question() const noexcept
{
    return display_question_;
}

EditorState EditorState::with_display_question(bool question) const
{
    EditorState next(*this);
    next.display_question_ = question;
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

const std::vector<ParkedTab> &EditorState::parked() const noexcept
{
    return parked_;
}

const std::vector<AutosaveId> &EditorState::autosave_ids() const noexcept
{
    return autosave_ids_;
}

std::size_t EditorState::parked_index_of(std::size_t position) const noexcept
{
    // 脇の列は帯の位置からアクティブを抜いた順なので、アクティブより右は 1 つずれる。
    return position < active_ ? position : position - 1;
}

std::optional<std::shared_ptr<const DocumentState>>
EditorState::loaded_at(std::size_t position) const
{
    return loaded_bundle(parked_.at(parked_index_of(position)));
}

std::optional<UnloadedDocument> EditorState::unloaded_at(std::size_t position) const
{
    if (position >= tab_count() || position == active_)
    {
        return std::nullopt;
    }
    return std::visit([](const auto &tab) { return unloaded_of(tab); },
                      parked_.at(parked_index_of(position)));
}

EditorState EditorState::with_restored(std::vector<UnloadedDocument> documents) const
{
    EditorState next(*this);
    for (UnloadedDocument &document : documents)
    {
        next.recency_ = core::tab_recency_opened(next.recency_, next.tab_count());
        next.parked_.emplace_back(std::make_shared<const UnloadedDocument>(std::move(document)));
        next.autosave_ids_.push_back(AutosaveId{next.next_autosave_id_++});
    }
    next.recency_ = core::tab_recency_touched(next.recency_, active_);
    return next;
}

EditorState EditorState::with_appended(std::vector<DocumentState> documents) const
{
    EditorState next(*this);
    for (DocumentState &document : documents)
    {
        next.recency_ = core::tab_recency_opened(next.recency_, next.tab_count());
        next.parked_.emplace_back(std::make_shared<const DocumentState>(std::move(document)));
        next.autosave_ids_.push_back(AutosaveId{next.next_autosave_id_++});
    }
    next.recency_ = core::tab_recency_touched(next.recency_, active_);
    return next;
}

EditorState EditorState::with_loaded(std::size_t position, DocumentState bundle) const
{
    if (!unloaded_at(position).has_value())
    {
        return *this;
    }
    EditorState next(*this);
    next.parked_.at(parked_index_of(position)) =
        std::make_shared<const DocumentState>(std::move(bundle));
    return next;
}

EditorState EditorState::with_dropped(std::size_t position) const
{
    if (!unloaded_at(position).has_value())
    {
        return *this;
    }
    EditorState next(*this);
    next.parked_.erase(std::next(next.parked_.begin(), distance_of(parked_index_of(position))));
    next.autosave_ids_.erase(std::next(next.autosave_ids_.begin(), distance_of(position)));
    next.recency_ = core::tab_recency_closed(recency_, position);
    next.active_ = position < active_ ? active_ - 1 : active_;
    return next;
}

EditorState EditorState::with_recency_ranked(std::span<const std::size_t> ranks) const
{
    if (ranks.size() != tab_count())
    {
        return *this;
    }
    // いちばん古いものから先頭へ動かしていくと、最後に動かした順位 0 が先頭になる。起動の 1
    // 回だけで 本数は一覧の上限（256）までなので、毎回まだ動かしていない中から探す。
    EditorState next(*this);
    std::vector<bool> touched(ranks.size(), false);
    for (std::size_t round = 0; round < ranks.size(); ++round)
    {
        const std::size_t oldest = oldest_untouched(ranks, touched);
        touched.at(oldest) = true;
        next.recency_ = core::tab_recency_touched(next.recency_, oldest);
    }
    return next;
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

const std::optional<std::size_t> &EditorState::close_request() const noexcept
{
    return close_request_;
}

EditorState EditorState::with_close_request(std::optional<std::size_t> position) const
{
    EditorState next(*this);
    next.close_request_ = position;
    return next;
}

const std::optional<core::EditorOperation> &EditorState::operation_request() const noexcept
{
    return operation_request_;
}

EditorState
EditorState::with_operation_request(std::optional<core::EditorOperation> operation) const
{
    EditorState next(*this);
    next.operation_request_ = operation;
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
    return std::make_shared<const DocumentState>(
        DocumentState{text_, core::collapsed_at(selection_.caret), std::move(history), document_,
                      scroll_.first_visible, vim_.wanted_column, vim_.scroll_lines, std::move(view),
                      scroll_.first_row, scroll_.horizontal, caret_affinity_, display_,
                      display_inquiry_, vim_marks_, language_, syntax_enabled_});
}

// 広げる（決定 1・4）。見えている行数は窓全体の値なので保ち、先頭行だけを文書から取る。
// Vim の文書ごとの値（欲しい列と 'scroll'）は engine の純関数が入れ替える。
void EditorState::spread(const DocumentState &tab)
{
    text_ = tab.text;
    selection_ = tab.selection;
    history_ = tab.history.sealed();
    document_ = tab.document;
    scroll_ = ScrollState{tab.first_visible, scroll_.visible_lines, tab.first_row, tab.horizontal};
    caret_affinity_ = tab.affinity;
    display_ = tab.display;
    display_inquiry_ = tab.inquiry;
    vim_marks_ = tab.marks;
    language_ = tab.language;
    syntax_enabled_ = tab.syntax_enabled;
    vim_ = core::vim_switched_document(vim_, tab.wanted_column, tab.scroll_lines);
}

EditorState EditorState::switched_to(std::size_t position, const DocumentState &target) const
{
    EditorState next(*this);
    // 今の文書を元の位置に置くと、脇の束の添字が帯の位置と一致する。
    next.parked_.insert(std::next(next.parked_.begin(), distance_of(active_)), parked_active());
    next.parked_.erase(std::next(next.parked_.begin(), distance_of(position)));
    next.active_ = position;
    next.spread(target);
    return next;
}

void EditorState::touch_active()
{
    recency_ = core::tab_recency_touched(recency_, active_);
    tab_walk_ = false;
}

EditorState EditorState::with_switched(std::size_t position) const
{
    if (position >= tab_count())
    {
        return *this;
    }
    // 歩きの途中でアクティブ自身を指されたら、着いた所で歩きを終える（ADR 0058 の決定 2）。
    if (position == active_)
    {
        EditorState next = with_walk_settled();
        next.touch_active();
        return next;
    }
    // まだ読んでいない文書へは切り替えない（読むのは controller・ADR 0059 の決定 5）。
    const auto target = loaded_at(position);
    if (!target.has_value())
    {
        return *this;
    }
    EditorState next = switched_to(position, *target.value());
    next.touch_active();
    return next;
}

EditorState EditorState::with_walked(std::size_t position) const
{
    if (position >= tab_count())
    {
        return *this;
    }
    if (position == active_)
    {
        EditorState next(*this);
        next.tab_walk_ = true;
        return next;
    }
    const auto target = loaded_at(position);
    if (!target.has_value())
    {
        return *this;
    }
    EditorState next = switched_to(position, *target.value());
    next.tab_walk_ = true;
    return next;
}

EditorState EditorState::with_walk_settled() const
{
    if (!tab_walk_)
    {
        return *this;
    }
    EditorState next(*this);
    next.touch_active();
    return next;
}

const core::TabRecency &EditorState::recency() const noexcept
{
    return recency_;
}

bool EditorState::tab_walking() const noexcept
{
    return tab_walk_;
}

EditorState EditorState::with_new_tab() const
{
    EditorState next(*this);
    next.parked_.insert(std::next(next.parked_.begin(), distance_of(active_)), parked_active());
    next.active_ = active_ + 1;
    next.autosave_ids_.insert(std::next(next.autosave_ids_.begin(), distance_of(next.active_)),
                              AutosaveId{next.next_autosave_id_++});
    next.spread(untitled_document());
    // 歩きの途中なら着いたタブで確定してから、新しいタブを先頭に置く（ADR 0058 の決定 2）。
    next.recency_ =
        core::tab_recency_opened(core::tab_recency_touched(recency_, active_), next.active_);
    next.tab_walk_ = false;
    return next;
}

EditorState EditorState::with_closed(std::size_t position) const
{
    if (position >= tab_count() || tab_count() == 1)
    {
        return *this;
    }
    EditorState next(*this);
    next.recency_ = core::tab_recency_closed(recency_, position);
    next.autosave_ids_.erase(std::next(next.autosave_ids_.begin(), distance_of(position)));
    if (position != active_)
    {
        // 脇の束は帯の位置からアクティブを抜いた順なので、右側は 1 つ左へずれる。
        next.parked_.erase(std::next(next.parked_.begin(), distance_of(parked_index_of(position))));
        next.active_ = position < active_ ? active_ - 1 : active_;
        next.touch_active();
        next.restart_other_showing(autosave_ids_.at(position));
        return next;
    }
    // 右隣は脇の束の添字 active_、左隣は active_ - 1。どちらも閉じた後の帯の位置と一致する。
    const std::size_t neighbour = active_ < parked_.size() ? active_ : active_ - 1;
    const auto target = loaded_bundle(parked_.at(neighbour));
    // 隣がまだ読んでいない文書なら閉じない（読むのは controller・ADR 0059 の決定 5）。
    if (!target.has_value())
    {
        return *this;
    }
    next.parked_.erase(std::next(next.parked_.begin(), distance_of(neighbour)));
    next.active_ = neighbour;
    next.spread(*target.value());
    next.touch_active();
    next.restart_other_showing(autosave_ids_.at(position));
    return next;
}

// ---------------------------------------------------------------- 枠（ADR 0148）

const SplitState &EditorState::split() const noexcept
{
    return split_;
}

PaneView EditorState::active_pane() const
{
    return PaneView{autosave_ids_.at(active_), selection_.caret,  caret_affinity_,
                    scroll_.first_visible,     scroll_.first_row, scroll_.horizontal,
                    vim_.wanted_column};
}

void EditorState::spread_pane(const PaneView &view)
{
    selection_ = core::collapsed_at(core::Offset{std::min(view.caret.value, text_.size_bytes())});
    caret_affinity_ = view.affinity;
    scroll_ =
        ScrollState{view.first_visible, scroll_.visible_lines, view.first_row, view.horizontal};
    vim_ = core::vim_switched_document(vim_, view.wanted_column, vim_.scroll_lines);
}

void EditorState::restart_other_showing(AutosaveId tab)
{
    const PaneView *other = pane_of(split_.other);
    if (other == nullptr || !(other->tab == tab))
    {
        return;
    }
    split_.other = PaneView{autosave_ids_.at(active_),
                            core::Offset{0},
                            core::CaretAffinity::leading,
                            core::LineNumber{first_line},
                            0,
                            0,
                            std::nullopt};
}

EditorState EditorState::with_split(core::SplitLayout layout) const
{
    if (layout == core::SplitLayout::none || split_.other.has_value())
    {
        return *this;
    }
    EditorState next(*this);
    next.split_ = SplitState{layout, core::PaneSide::first, active_pane()};
    return next;
}

EditorState EditorState::with_pane_swapped() const
{
    const PaneView *shown = pane_of(split_.other);
    // プレビューの枠へは移らない（ADR 0155 の決定 4・知らせは controller）。
    if (shown == nullptr)
    {
        return *this;
    }
    const PaneView other = *shown;
    const PaneView leaving = active_pane();
    if (other.tab == leaving.tab)
    {
        // 同じ文書は動かさない。枠を移ると INSERT の単位も閉じる（タブの切り替えと同じ）。
        EditorState next(*this);
        next.history_ = history_.sealed();
        next.spread_pane(other);
        next.split_.other = leaving;
        next.split_.active_side = opposite_side(split_.active_side);
        return next;
    }
    const auto position = position_of(autosave_ids_, other.tab);
    const auto target = position.and_then([this](std::size_t at) { return loaded_at(at); });
    // まだ読んでいない文書へは移らない（読むのは controller の reach_tab・決定 2）。
    if (!position.has_value() || position.value() == active_ || !target.has_value())
    {
        return *this;
    }
    EditorState next = switched_to(position.value(), *target.value());
    next.touch_active();
    next.spread_pane(other);
    next.split_.other = leaving;
    next.split_.active_side = opposite_side(split_.active_side);
    return next;
}

EditorState EditorState::without_split() const
{
    EditorState next(*this);
    next.split_ = SplitState{};
    return next;
}

EditorState EditorState::with_other_adjusted(const core::Edit &edit,
                                             const core::TextBuffer &after) const
{
    // プレビューは編集でずらさない（錨は次の frame で追従し直す・ADR 0155 の決定 12）。
    const PaneView *shown = pane_of(split_.other);
    if (shown == nullptr || !(shown->tab == autosave_ids_.at(active_)))
    {
        return *this;
    }
    EditorState next(*this);
    PaneView other = *shown;
    const core::Offset start = core::offset_after_edit(text_.line_start(other.first_visible), edit);
    const core::TextPosition top = after.position_of(start);
    // 先頭行の行頭が削除の中に畳まれて別の行の途中へ来たら、画面行も行の先頭へ戻す。
    if (top.column.value != 0)
    {
        other.first_row = 0;
    }
    other.first_visible = top.line;
    other.caret = core::offset_after_edit(other.caret, edit);
    next.split_.other = other;
    return next;
}

EditorState EditorState::with_other_scrolled(core::LineNumber first_visible,
                                             std::size_t first_row) const
{
    const PaneView *shown = pane_of(split_.other);
    if (shown == nullptr)
    {
        return *this;
    }
    PaneView other = *shown;
    other.first_visible = first_visible;
    other.first_row = first_row;
    EditorState next(*this);
    next.split_.other = other;
    return next;
}

const PaneView *EditorState::other_pane() const noexcept
{
    return pane_of(split_.other);
}

const PreviewView *EditorState::preview() const noexcept
{
    return preview_of(split_.other);
}

EditorState EditorState::with_preview(PreviewView view) const
{
    EditorState next(*this);
    if (split_.layout == core::SplitLayout::none)
    {
        next.split_.layout = core::SplitLayout::vertical;
        next.split_.active_side = core::PaneSide::first;
    }
    next.split_.other = std::move(view);
    return next;
}

std::optional<EditorState> EditorState::other_pane_state() const
{
    const PaneView *shown = pane_of(split_.other);
    if (shown == nullptr)
    {
        return std::nullopt;
    }
    const PaneView &other = *shown;
    EditorState pane(*this);
    if (!(other.tab == autosave_ids_.at(active_)))
    {
        const auto position = position_of(autosave_ids_, other.tab);
        const auto target = position.and_then([this](std::size_t at) { return loaded_at(at); });
        if (!target.has_value())
        {
            return std::nullopt;
        }
        pane.spread(*target.value());
    }
    pane.spread_pane(other);
    pane.composition_.reset();
    pane.command_input_.reset();
    pane.search_preview_.reset();
    return pane;
}
} // namespace nenenib::application
