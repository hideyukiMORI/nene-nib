// 起動画面（ADR 0172・追補 2026-11-11）。出る条件・選べる行・鍵の振り分け・行の実行・履歴と
// ブックマークの 1 回の読みはこの翻訳単位だけが持つ。StartListPort を呼ぶのはここだけで、最初の
// frame の合図（FirstFramePresented）の時点で起動画面が出ているときに 1 回だけ頼む（決定 3・起動と
// 開く・切り替え・新しいタブ・保存・打鍵の道は呼ばない）。
#include "EditorController.hpp"

#include "EditorOperation.hpp"
#include "GuideVisibility.hpp"
#include "PaletteOrigin.hpp"
#include "StartScreen.hpp"
#include "StartScreenRoute.hpp"
#include "StartStep.hpp"
#include "VimMode.hpp"

#include <algorithm>
#include <array>
#include <span>
#include <utility>
#include <variant>

namespace nenenib::application
{
namespace
{
// Vim の NORMAL で、次の鍵を待つものが何も無いか（追補の 5・オペレータ待ちと回数とレジスタの
// 選びと文字待ちの間は出さない）。
[[nodiscard]] bool vim_resting(const core::VimState &vim) noexcept
{
    return vim.mode == core::VimMode::normal && !vim.count.has_value() &&
           !vim.pending.has_value() && !vim.input_wait.has_value() &&
           !vim.selected_register.has_value() && !vim.register_count.has_value();
}

// 選んでいるときだけ受ける鍵（Enter と Esc）の行き先。
[[nodiscard]] StartScreenRoute when_selected(bool selected, StartScreenRoute route) noexcept
{
    return selected ? route : StartScreenRoute::pass;
}

// Vim の特別な鍵の行き先。表に無い鍵は本文へ。Enter と Esc は選んでいるときだけ受ける。
using StartSpecialRow = std::pair<core::VimSpecialKey, StartScreenRoute>;
constexpr std::array<StartSpecialRow, 4> start_vim_specials{{
    {core::VimSpecialKey::arrow_down, StartScreenRoute::next},
    {core::VimSpecialKey::arrow_up, StartScreenRoute::previous},
    {core::VimSpecialKey::enter, StartScreenRoute::run},
    {core::VimSpecialKey::escape, StartScreenRoute::clear},
}};

// Vim の鍵の行き先。NORMAL の `j` `k` と矢印は選び、Enter と Esc は選んでいるときだけ受ける。
[[nodiscard]] StartScreenRoute vim_key_route(const core::VimKey &key, bool selected) noexcept
{
    if (const auto *character = std::get_if<core::VimCharacter>(&key); character != nullptr)
    {
        if (character->code == U'j')
        {
            return StartScreenRoute::next;
        }
        return character->code == U'k' ? StartScreenRoute::previous : StartScreenRoute::pass;
    }
    const auto *special = std::get_if<core::VimSpecialKey>(&key);
    if (special == nullptr)
    {
        return StartScreenRoute::pass;
    }
    const auto found = std::ranges::find(start_vim_specials, *special, &StartSpecialRow::first);
    if (found == start_vim_specials.end())
    {
        return StartScreenRoute::pass;
    }
    const bool selecting =
        found->second == StartScreenRoute::next || found->second == StartScreenRoute::previous;
    return selecting ? found->second : when_selected(selected, found->second);
}

// 通常モードの ↑ ↓（Shift なし）の行き先。
[[nodiscard]] StartScreenRoute motion_route(const MoveCaret &move) noexcept
{
    if (move.anchoring != core::SelectionAnchoring::collapse)
    {
        return StartScreenRoute::pass;
    }
    if (move.motion == core::CaretMotion::next_line)
    {
        return StartScreenRoute::next;
    }
    return move.motion == core::CaretMotion::previous_line ? StartScreenRoute::previous
                                                           : StartScreenRoute::pass;
}

// 起動画面が出ている間の意図の行き先（追補の 5）。文字・貼り付け・IME の確定・ほかの鍵は本文へ。
[[nodiscard]] StartScreenRoute start_screen_route(const EditorIntent &intent,
                                                  bool selected) noexcept
{
    if (const auto *press = std::get_if<VimKeyPress>(&intent); press != nullptr)
    {
        return vim_key_route(press->key, selected);
    }
    if (const auto *move = std::get_if<MoveCaret>(&intent); move != nullptr)
    {
        return motion_route(*move);
    }
    if (std::holds_alternative<NewLine>(intent))
    {
        return when_selected(selected, StartScreenRoute::run);
    }
    if (std::holds_alternative<CancelSelection>(intent))
    {
        return when_selected(selected, StartScreenRoute::clear);
    }
    return StartScreenRoute::pass;
}
} // namespace

// ---------------------------------------------------------------- 起動画面（ADR 0172）

// 決定 1 と追補 2026-11-11 の 1〜5。guide=hidden・入力行（`:`・検索・Ctrl+P の面）・IME の変換中・
// 録画中・全面プレビューの間は出さない。アクティブな文書が何も書いていない無題で、Vim なら NORMAL
// で次の鍵を待っていないときだけ出す。エクスプローラーの焦点と知らせの 1 行は条件に入れない（描いた
// まま）。状態は持たない。
bool EditorController::start_screen_shown() const noexcept
{
    if (state_.settings().guide == core::GuideVisibility::hidden ||
        state_.command_input().has_value() || state_.composition().has_value() ||
        state_.vim().macro_recording.has_value() || state_.full_preview() != nullptr)
    {
        return false;
    }
    if (state_.document().path.has_value() || state_.text().size_bytes() != 0 ||
        state_.history().size() != 0)
    {
        return false;
    }
    switch (state_.mode())
    {
    case core::EditMode::ordinary:
        return true;
    case core::EditMode::vim:
        return vim_resting(state_.vim()) && state_.key_map_pending().empty();
    }
    return false;
}

std::vector<core::StartEntry> EditorController::start_screen_entries() const
{
    if (!start_lists_.has_value())
    {
        return core::start_entries({}, {});
    }
    const auto &lists = start_lists_.value();
    return core::start_entries(std::span<const core::FilePath>(lists.recent),
                               std::span<const core::FilePath>(lists.bookmarks));
}

std::optional<StartScreenView> EditorController::start_screen() const
{
    return start_screen_view();
}

std::optional<StartScreenView> EditorController::start_screen_view() const
{
    if (!start_screen_shown())
    {
        return std::nullopt;
    }
    auto entries = start_screen_entries();
    const auto selected = start_selection_.has_value() && start_selection_.value() < entries.size()
                              ? start_selection_
                              : std::nullopt;
    return StartScreenView{std::move(entries), selected, start_lists_.has_value(),
                           core::start_key_entries(state_.mode())};
}

// 決定 3。最初の 1 回だけ。そのとき起動画面が出ていなければ（引数のファイル・前回のタブで起動した）
// 読みを頼まない（後で無題のタブを足しても読まない・起動画面は「はじめる」と鍵だけ）。
void EditorController::accept(const FirstFramePresented &)
{
    repaint_ = false;
    if (first_frame_presented_)
    {
        return;
    }
    first_frame_presented_ = true;
    if (start_screen_shown() && !start_lists_requested_)
    {
        start_lists_requested_ = true;
        ports_.start_lists.request();
    }
}

void EditorController::collect_start_lists()
{
    if (!start_lists_requested_ || start_lists_.has_value())
    {
        return;
    }
    auto lists = ports_.start_lists.collect();
    if (!lists.has_value())
    {
        return;
    }
    start_lists_ = std::move(lists);
    // 行の列が変わるので選びは「何も選んでいない」に戻す（位置が別の行を指さない）。
    start_selection_.reset();
}

bool EditorController::route_start_screen(const EditorIntent &intent)
{
    if (!first_frame_presented_ || explorer_focused() || !start_screen_shown())
    {
        return false;
    }
    const auto entries = start_screen_entries();
    if (start_selection_.has_value() && start_selection_.value() >= entries.size())
    {
        start_selection_.reset();
    }
    const auto route = start_screen_route(intent, start_selection_.has_value());
    switch (route)
    {
    case StartScreenRoute::pass:
        return false;
    case StartScreenRoute::next:
        start_selection_ =
            core::start_selection_stepped(start_selection_, entries.size(), core::StartStep::next);
        return true;
    case StartScreenRoute::previous:
        start_selection_ = core::start_selection_stepped(start_selection_, entries.size(),
                                                         core::StartStep::previous);
        return true;
    case StartScreenRoute::run:
        // run は選んでいるときだけ来る（when_selected）。選びを外してから実行する。
        run_start_entry(entries.at(std::exchange(start_selection_, std::nullopt).value_or(0)));
        return true;
    case StartScreenRoute::clear:
        start_selection_.reset();
        return true;
    }
    return false;
}

void EditorController::accept(const ChooseStartEntry &intent)
{
    if (!first_frame_presented_ || !start_screen_shown())
    {
        return;
    }
    const auto entries = start_screen_entries();
    if (intent.index >= entries.size())
    {
        return;
    }
    start_selection_.reset();
    run_start_entry(entries.at(intent.index));
}

// 決定 4。ファイルは一覧から開くのと同じ open_listed の 1 本（履歴の行は開けなければ履歴から外す・
// ブックマークの行は知らせて残す・ADR 0060 / 0063）。「ファイルを開く」「フォルダを開く」は操作の
// 頼み（ui が run_operation で Ctrl+O と choose_explorer_root を通す・ADR 0078 の決定 8）。「新しい
// ファイル」はそのまま打ち始める行なので、選びを外すだけ。
void EditorController::run_start_entry(const core::StartEntry &entry)
{
    if (const auto &path = entry.path; path.has_value())
    {
        const bool recent = entry.kind == core::StartEntryKind::recent;
        open_listed(path.value(),
                    recent ? core::PaletteOrigin::history : core::PaletteOrigin::bookmark);
        // 開けずに履歴から外したファイルは、読んだ列からも外す（起動画面が出たままのとき）。
        if (recent && start_screen_shown() && start_lists_.has_value())
        {
            std::erase(start_lists_.value().recent, path.value());
        }
        return;
    }
    switch (entry.kind)
    {
    case core::StartEntryKind::recent:
    case core::StartEntryKind::bookmark:
    case core::StartEntryKind::new_file:
        return;
    case core::StartEntryKind::open_file:
        state_ = state_.with_operation_request(core::EditorOperation::open_file);
        return;
    case core::StartEntryKind::open_folder:
        state_ = state_.with_operation_request(core::EditorOperation::choose_explorer_root);
        return;
    }
}

void EditorController::settle_start_screen() noexcept
{
    if (!start_screen_shown())
    {
        start_selection_.reset();
    }
}
} // namespace nenenib::application
