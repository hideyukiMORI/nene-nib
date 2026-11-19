// タブの右クリックのメニュー（ADR 0078 の追補 #1790・施主決定 D98）。開く・選んだ行の実行・帯が
// 変わったら閉じる、とエクスプローラーで場所を開く頼みの失敗の受け取りの群。配置・描画・選び方・
// 閉じ方は本文の右クリックのメニュー（EditorControllerContextMenu.cpp）と同じ部品で、項目の列と
// 使えるかは core の tab_menu_items の 1 本。項目はどれもタブの退避番号で名指ししたタブに当て、
// 保存と名前を付けて保存だけが先にそのタブへ切り替えて ui の run_operation の 1 本に渡す。閉じる 3
// つ は ui の閉じる道（× と同じ・未保存なら「保存しますか」）に頼み、controller はタブを閉じない。
#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "AutosaveId.hpp"
#include "ContextMenu.hpp"
#include "ContextMenuKind.hpp"
#include "ContextMenuMode.hpp"
#include "ContextMenuOpen.hpp"
#include "CopyTabName.hpp"
#include "DocumentState.hpp"
#include "EditorOperation.hpp"
#include "EditorState.hpp"
#include "FilePath.hpp"
#include "OpenTabMenu.hpp"
#include "RevealFailure.hpp"
#include "RevealTab.hpp"
#include "SweptTab.hpp"
#include "SwitchTab.hpp"
#include "TabMenuSituation.hpp"
#include "TabNamePart.hpp"
#include "TabSweep.hpp"
#include "TabSweepSide.hpp"
#include "UiMessage.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::application
{
using detail::tab_path_at;

namespace
{
// 帯の位置 position のタブの中身を読んであるか（まだ読んでいないタブ UnloadedDocument でない）。
// 脇の束は帯の位置からアクティブを抜いた順。
[[nodiscard]] bool tab_loaded(const EditorState &state, std::size_t position)
{
    if (position == state.active_tab())
    {
        return true;
    }
    const std::size_t parked = position < state.active_tab() ? position : position - 1;
    return std::holds_alternative<std::shared_ptr<const DocumentState>>(state.parked().at(parked));
}

[[nodiscard]] core::UiMessage reveal_notice(RevealFailure failure) noexcept
{
    switch (failure)
    {
    case RevealFailure::not_found:
        return core::UiMessage::notice_reveal_not_found;
    case RevealFailure::failed:
        return core::UiMessage::notice_reveal_failed;
    }
    std::unreachable();
}
} // namespace

const std::vector<AutosaveId> &EditorController::tab_ids() const noexcept
{
    return state_.autosave_ids();
}

std::size_t EditorController::active_tab() const noexcept
{
    return state_.active_tab();
}

void EditorController::accept(const OpenTabMenu &intent)
{
    // 入力行・面・変換の間は開かない（本文のメニューと同じ・ui もこの間は送らない）。
    if (state_.command_input().has_value() || state_.composition().has_value() ||
        intent.tab >= state_.tab_count())
    {
        return;
    }
    const core::TabMenuSituation situation{tab_path_at(state_, intent.tab).has_value(),
                                           tab_loaded(state_, intent.tab), state_.tab_count() > 1,
                                           intent.tab + 1 < state_.tab_count()};
    const auto items = core::tab_menu_items(situation);
    context_menu_ = ContextMenuOpen{intent.x,
                                    intent.y,
                                    core::ContextMenuKind::tab,
                                    core::ContextMenuMode::ordinary,
                                    {items.begin(), items.end()},
                                    std::nullopt,
                                    state_.autosave_ids().at(intent.tab),
                                    state_.autosave_ids()};
}

void EditorController::accept(const CopyTabName &intent)
{
    copy_tab_name_at(state_.active_tab(), intent.part);
}

void EditorController::accept(const RevealTab &)
{
    reveal_tab_at(state_.active_tab());
}

// 操作 → タブに当てる関数の表（#1790）。private の関数を指すので、表は static メンバ関数の中に
// 置く。表に無い操作はタブのメニューに載らない（tab_menu の列と同じ 8 つ）。
EditorController::TabMenuRunner
EditorController::tab_menu_runner(core::EditorOperation operation) noexcept
{
    // 1 行は操作と関数（行の型を足さず std::pair の別名・CPP-011）。
    using TabMenuRow = std::pair<core::EditorOperation, TabMenuRunner>;
    static constexpr std::array<TabMenuRow, 8> runners{{
        {core::EditorOperation::reveal_in_explorer, &EditorController::reveal_tab_at},
        {core::EditorOperation::copy_path, &EditorController::copy_tab_path_at},
        {core::EditorOperation::copy_file_name, &EditorController::copy_tab_file_name_at},
        {core::EditorOperation::save, &EditorController::save_tab_at},
        {core::EditorOperation::save_as, &EditorController::save_tab_as_at},
        {core::EditorOperation::close_tab, &EditorController::close_tab_at},
        {core::EditorOperation::close_other_tabs, &EditorController::close_tabs_beside_at},
        {core::EditorOperation::close_right_tabs, &EditorController::close_tabs_right_at},
    }};
    static_assert(runners.size() == core::tab_menu_size,
                  "every tab menu row has a runner and nothing else does");
    const auto found = std::ranges::find(runners, operation, &TabMenuRow::first);
    return found == runners.end() ? nullptr : found->second;
}

void EditorController::run_tab_menu_row(core::EditorOperation operation, AutosaveId tab)
{
    const auto runner = tab_menu_runner(operation);
    const auto position = tab_position(state_.autosave_ids(), tab);
    if (runner != nullptr && position.has_value())
    {
        (this->*runner)(position.value());
    }
}

// まだ読んでいないタブも読まずにパスで頼む。無題は頼まずに知らせる（メニューでは薄い・F1 の一覧
// から選んだとき）。
void EditorController::reveal_tab_at(std::size_t tab)
{
    const auto path = tab_path_at(state_, tab);
    if (!path.has_value())
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_tab_untitled));
        return;
    }
    ports_.reveal.reveal(path.value());
    ++reveals_pending_;
}

void EditorController::copy_tab_path_at(std::size_t tab)
{
    copy_tab_name_at(tab, TabNamePart::path);
}

void EditorController::copy_tab_file_name_at(std::size_t tab)
{
    copy_tab_name_at(tab, TabNamePart::file_name);
}

// 切り替えずに写す。無題は「無題」に当たる題名を写さずに知らせる。書けなければ 1 行の知らせ
// （clipboard_written・ADR 0134）。
void EditorController::copy_tab_name_at(std::size_t tab, TabNamePart part)
{
    const auto path = tab_path_at(state_, tab);
    if (!path.has_value())
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_tab_untitled));
        return;
    }
    std::string_view text;
    switch (part)
    {
    case TabNamePart::path:
        text = path.value().text();
        break;
    case TabNamePart::file_name:
        text = path.value().file_name();
        break;
    }
    static_cast<void>(clipboard_written(text));
}

void EditorController::save_tab_at(std::size_t tab)
{
    run_on_tab(tab, core::EditorOperation::save);
}

void EditorController::save_tab_as_at(std::size_t tab)
{
    run_on_tab(tab, core::EditorOperation::save_as);
}

// 保存の 2 つはアクティブな文書に効く今の道（ui の save_document / save_document_as）なので、先に
// そのタブへ切り替える。切り替えが成らなければ（裏の読みの途中・読めずに外れた）何もしない。
void EditorController::run_on_tab(std::size_t tab, core::EditorOperation operation)
{
    const AutosaveId target = state_.autosave_ids().at(tab);
    if (tab != state_.active_tab())
    {
        accept(SwitchTab{tab});
    }
    if (state_.autosave_ids().at(state_.active_tab()) == target)
    {
        state_ = state_.with_operation_request(operation);
    }
}

// × と同じ閉じる流れ（未保存なら切り替えて「保存しますか」・最後の 1 本なら窓を閉じる）は ui が
// 持つ。controller は頼みを 1 意図だけ立てる（`:tabclose` と同じ close_request）。
void EditorController::close_tab_at(std::size_t tab)
{
    state_ = state_.with_close_request(tab);
}

void EditorController::close_tabs_beside_at(std::size_t tab)
{
    state_ = state_.with_tab_sweep_request(
        TabSweep{state_.autosave_ids().at(tab), TabSweepSide::others});
}

void EditorController::close_tabs_right_at(std::size_t tab)
{
    state_ =
        state_.with_tab_sweep_request(TabSweep{state_.autosave_ids().at(tab), TabSweepSide::right});
}

void EditorController::settle_context_menu()
{
    if (context_menu_.has_value() && context_menu_.value().kind == core::ContextMenuKind::tab &&
        context_menu_.value().band != state_.autosave_ids())
    {
        context_menu_.reset();
    }
}

void EditorController::collect_reveals()
{
    if (reveals_pending_ == 0)
    {
        return;
    }
    const auto results = ports_.reveal.collect();
    reveals_pending_ -= std::min(reveals_pending_, results.size());
    for (const auto &result : results)
    {
        if (!result.has_value())
        {
            state_ = state_.with_command_message(ui_notice(reveal_notice(result.error())));
        }
    }
}
} // namespace nenenib::application
