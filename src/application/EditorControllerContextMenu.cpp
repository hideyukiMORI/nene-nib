// 本文の右クリックのメニュー（ADR 0078 の追補 #1051・D71・FR-031）。開く・選ぶ・閉じる・実行の群。
// 項目の列と使えるかと鍵の表示は core の context_menu_items / context_menu_key の 1 本で、ここは
// 開いた所の種類（ContextMenuMode）を決めて状態に置くだけ。実行は Vim の NORMAL と VISUAL では表の
// Vim の鍵の列を map の右辺と同じ道（delivered_keys）で engine に流し、ほかは operation_request を
// 立てて ui の run_operation の 1 本に渡す。
#include "EditorController.hpp"

#include "ContextMenu.hpp"
#include "ContextMenuItem.hpp"
#include "ContextMenuKind.hpp"
#include "ContextMenuMode.hpp"
#include "ContextMenuOpen.hpp"
#include "ContextMenuRow.hpp"
#include "ContextMenuSituation.hpp"
#include "ContextMenuView.hpp"
#include "EditMode.hpp"
#include "EditorControllerDetail.hpp"
#include "KeyMap.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OperationTexts.hpp"
#include "PointText.hpp"
#include "PointerClicks.hpp"
#include "PreviewSelectionText.hpp"
#include "PreviewView.hpp"
#include "SelectionAnchoring.hpp"
#include "UiTexts.hpp"
#include "VimMode.hpp"
#include "VimStep.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
namespace
{
// メニューの意図（開いたまま受ける側）。
[[nodiscard]] bool is_menu_intent(const EditorIntent &intent) noexcept
{
    return std::holds_alternative<OpenContextMenu>(intent) ||
           std::holds_alternative<OpenTabMenu>(intent) ||
           std::holds_alternative<PointContextMenu>(intent) ||
           std::holds_alternative<StepContextMenu>(intent) ||
           std::holds_alternative<ChooseContextMenu>(intent) ||
           std::holds_alternative<CloseContextMenu>(intent);
}

[[nodiscard]] core::ContextMenuMode vim_menu_mode(core::VimMode mode) noexcept
{
    switch (mode)
    {
    case core::VimMode::insert:
        return core::ContextMenuMode::vim_insert;
    case core::VimMode::normal:
        return core::ContextMenuMode::vim_normal;
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        return core::ContextMenuMode::vim_visual;
    }
    std::unreachable();
}

// from の次（down）か前の使える行。回り込み、使える行が無ければ無し。
[[nodiscard]] std::optional<std::size_t>
stepped_row(const std::vector<core::ContextMenuItem> &items, std::optional<std::size_t> from,
            bool down) noexcept
{
    const std::size_t count = items.size();
    if (count == 0)
    {
        return std::nullopt;
    }
    std::size_t at = from.value_or(down ? count - 1 : 0);
    for (std::size_t tried = 0; tried < count; ++tried)
    {
        at = down ? (at + 1) % count : (at + count - 1) % count;
        if (items.at(at).enabled)
        {
            return at;
        }
    }
    return std::nullopt;
}
} // namespace

bool EditorController::context_menu_active() const noexcept
{
    return context_menu_.has_value();
}

std::optional<std::size_t> EditorController::context_menu_highlighted() const noexcept
{
    return context_menu_.has_value() ? context_menu_.value().highlighted : std::nullopt;
}

void EditorController::close_context_menu_before(const EditorIntent &intent)
{
    if (context_menu_.has_value() && !is_menu_intent(intent) && !detail::not_user_operation(intent))
    {
        context_menu_.reset();
    }
}

core::ContextMenuMode EditorController::context_menu_mode() const noexcept
{
    if (state_.full_preview() != nullptr)
    {
        return core::ContextMenuMode::reading;
    }
    switch (state_.mode())
    {
    case core::EditMode::ordinary:
        return core::ContextMenuMode::ordinary;
    case core::EditMode::vim:
        break;
    }
    return vim_menu_mode(state_.vim().mode);
}

// 選択の中（端を含む）を押したなら選択を保つ。外ならクリックと同じ道でキャレットを置く。
void EditorController::place_caret_for_menu(const PointText &pointed)
{
    const auto selected = highlighted_range();
    if (!core::is_empty(selected))
    {
        const auto caret = pointed_caret(pointed);
        if (!caret.has_value())
        {
            return;
        }
        const auto at = state_.text().offset_of(caret.value().position);
        if (!(at < selected.begin || selected.end < at))
        {
            return;
        }
    }
    // 左クリックでキャレットを置くときと同じく、狙いの横位置を捨てる（keeps_wanted_x は保つ側）。
    wanted_x_.reset();
    accept(PointText{pointed.x, pointed.row, core::SelectionAnchoring::collapse,
                     PointerClicks::single});
}

void EditorController::accept(const OpenContextMenu &intent)
{
    // 入力行・面・変換の間は開かない（ui もこの間は送らない）。
    if (state_.command_input().has_value() || explorer_focused() ||
        state_.composition().has_value())
    {
        return;
    }
    auto mode = context_menu_mode();
    if (mode == core::ContextMenuMode::ordinary && intent.pointed.has_value())
    {
        place_caret_for_menu(intent.pointed.value());
    }
    bool selected = !core::is_empty(highlighted_range());
    if (mode == core::ContextMenuMode::reading)
    {
        const PreviewView *preview = state_.shown_preview();
        selected = preview != nullptr && preview->document != nullptr &&
                   preview->selection.has_value() &&
                   core::preview_selection_range(*preview->document, preview->selection.value())
                       .has_value();
    }
    const auto items = core::context_menu_items(core::ContextMenuSituation{mode, selected});
    context_menu_ = ContextMenuOpen{intent.x,
                                    intent.y,
                                    core::ContextMenuKind::body,
                                    mode,
                                    {items.begin(), items.end()},
                                    std::nullopt,
                                    std::nullopt,
                                    {}};
}

void EditorController::accept(const PointContextMenu &intent)
{
    if (!context_menu_.has_value())
    {
        return;
    }
    auto &menu = context_menu_.value();
    const bool usable = intent.row.has_value() && intent.row.value() < menu.items.size() &&
                        menu.items.at(intent.row.value()).enabled;
    menu.highlighted = usable ? intent.row : std::nullopt;
}

void EditorController::accept(const StepContextMenu &intent)
{
    if (!context_menu_.has_value())
    {
        return;
    }
    auto &menu = context_menu_.value();
    menu.highlighted = stepped_row(menu.items, menu.highlighted, intent.down);
}

void EditorController::accept(const ChooseContextMenu &intent)
{
    if (!context_menu_.has_value())
    {
        return;
    }
    const auto row = intent.row.has_value() ? intent.row : context_menu_.value().highlighted;
    if (!row.has_value() || row.value() >= context_menu_.value().items.size() ||
        !context_menu_.value().items.at(row.value()).enabled)
    {
        return;
    }
    const auto chosen = std::move(context_menu_).value();
    context_menu_.reset();
    run_chosen_row(chosen, row.value());
}

// 本文のメニューは本文の道へ、タブのメニューは右クリックしたタブ（退避番号で引き直す）へ。
void EditorController::run_chosen_row(const ContextMenuOpen &menu, std::size_t row)
{
    switch (menu.kind)
    {
    case core::ContextMenuKind::body:
        run_context_menu_row(menu.items.at(row), menu.mode);
        return;
    case core::ContextMenuKind::tab:
        if (menu.tab.has_value())
        {
            run_tab_menu_row(menu.items.at(row).operation, menu.tab.value());
        }
        return;
    }
    std::unreachable();
}

void EditorController::accept(const CloseContextMenu &)
{
    context_menu_.reset();
}

void EditorController::run_context_menu_row(const core::ContextMenuItem &item,
                                            core::ContextMenuMode mode)
{
    run_operation_keys(item.operation, core::context_menu_vim_keys(item.operation, mode));
}

// F1 の一覧から選んだ操作（追補 #1051 の 7a）。Vim の NORMAL / VISUAL の 5 つだけ Vim の鍵の列。
void EditorController::run_listed_operation(core::EditorOperation operation)
{
    const auto mode = context_menu_mode();
    run_operation_keys(operation, core::listed_vim_routed(operation, mode)
                                      ? core::context_menu_vim_keys(operation, mode)
                                      : std::string_view{});
}

// メニューと F1 の一覧の実行の入口の 1 本（追補 #1051）。Vim の鍵の列があれば、待ちの状態（回数・
// オペレータ・1 鍵の引数待ち・`"` のレジスタ・`q` の名前待ち）を vim_operator_cleared で捨ててから
// （VISUAL と INSERT の Ctrl-o の印は保つ・ADR 0182 の決定 8・Vim の `:amenu` が先に `<C-C>` を
// 入れるのと同じ）Vim で同じ鍵を打ったのと同じく流す（`"+` は step_vim の
// 直前にクリップボードから写す・ADR 0051）。無ければ ui の run_operation（鍵と同じ 1 本）。
void EditorController::run_operation_keys(core::EditorOperation operation, std::string_view keys)
{
    if (keys.empty())
    {
        state_ = state_.with_operation_request(operation);
        return;
    }
    // 表の列は試験が全部読めることを確かめている（読めない列は何も流さない）。
    const auto parsed = core::vim_keys_of_notation(keys);
    if (!parsed.has_value())
    {
        return;
    }
    state_ = state_.with_vim(core::vim_operator_cleared(state_.vim()));
    static_cast<void>(delivered_keys(parsed.value()));
}

std::optional<ContextMenuView> EditorController::context_menu_view() const
{
    if (!context_menu_.has_value())
    {
        return std::nullopt;
    }
    const auto &menu = context_menu_.value();
    const auto language = state_.display_language();
    ContextMenuView view{menu.x, menu.y, menu.kind, {}, menu.highlighted};
    view.rows.reserve(menu.items.size());
    for (const auto &item : menu.items)
    {
        view.rows.push_back(ContextMenuRow{
            item.operation,
            std::string(core::ui_text(core::operation_text(item.operation).name, language)),
            menu.kind == core::ContextMenuKind::tab
                ? core::tab_menu_key(item.operation, state_.mode())
                : core::context_menu_key(item.operation, menu.mode),
            item.enabled, item.separated});
    }
    return view;
}
} // namespace nenenib::application
