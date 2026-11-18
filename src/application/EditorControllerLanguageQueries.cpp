#include "EditorController.hpp"

#include "EditMode.hpp"
#include "EditorControllerDetail.hpp"
#include "EditorIntent.hpp"
#include "FileUri.hpp"
#include "HoverTip.hpp"
#include "LanguageAnswers.hpp"
#include "LanguageDefinition.hpp"
#include "LanguageHover.hpp"
#include "LanguageQueryKind.hpp"
#include "LspLocation.hpp"
#include "LspRequestKind.hpp"
#include "OpenedAs.hpp"
#include "ProjectHitLanding.hpp"
#include "QueryLanguage.hpp"
#include "UiMessage.hpp"
#include "VimLanguageQuery.hpp"
#include "VimMode.hpp"
#include "VimState.hpp"
#include "WindowDeactivated.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::application
{
// ------------------------------------- 言語サーバーへの問いと答えの使い方（ADR 0190 の決定 6〜8）

// 操作「定義へ移動」（F12）と「情報を表示」。言語サーバーが動いていない文書（要求を出せない文書）
// では 1 行知らせる。
void EditorController::accept(const QueryLanguage &intent)
{
    if (!query_language(intent.kind))
    {
        state_ = state_.with_command_message(
            ui_notice(core::UiMessage::notice_language_server_not_running));
    }
}

// `gd` `K`。出せなければ何もしない（engine の歩は表に行が無かったときと同じ失敗のまま）。
void EditorController::perform(const core::VimLanguageQuery &effect)
{
    static_cast<void>(query_language(effect.kind));
}

// 問いを出す 1 本（段 A の入口へ）。出せたら真。hover の前の答えは捨てる（札も消える）。
bool EditorController::query_language(core::LanguageQueryKind kind)
{
    switch (kind)
    {
    case core::LanguageQueryKind::definition:
        definition_anchor_ = state_.selection().anchor;
        return request_definition();
    case core::LanguageQueryKind::hover:
        drop_hover();
        return request_hover();
    }
    std::unreachable();
}

// 窓が非アクティブになったら札を消す（決定 6）。札が無ければ描き直さない。
void EditorController::accept(const WindowDeactivated &)
{
    window_inactive_ = true;
    repaint_ = state_.language_answers().hover.has_value();
    drop_hover();
}

// 札を消す意図を決める 1 か所（決定 6）。使う人の操作（鍵・クリック・ホイール・タブの切り替え…・
// not_user_operation でない意図）は写す前に消す。合図・窓の寸法・帯の hover は消さない（窓が
// 非アクティブになった知らせは accept が消す）。答えが無ければ欄を 1 つ読んで旗を 1 つ書くだけ。
void EditorController::settle_hover(const EditorIntent &intent)
{
    if (detail::not_user_operation(intent))
    {
        return;
    }
    // 使う人の操作が届くのは窓がアクティブなとき。
    window_inactive_ = false;
    drop_hover();
}

void EditorController::drop_hover()
{
    if (!state_.language_answers().hover.has_value())
    {
        return;
    }
    LanguageAnswers answers = state_.language_answers();
    answers.hover.reset();
    state_ = state_.with_language_answers(std::move(answers));
}

// frame に載せる札の文（決定 6）。答えの文脈（文書・revision・キャレット）が今と同じ間だけ出る
// （本文の変更とタブの切り替えは、どの道から来てもここで消える）。12 行を越えた分は core が切る。
std::optional<std::string> EditorController::shown_hover() const
{
    const auto &hover = state_.language_answers().hover;
    if (!hover.has_value() || !hover.value().text.has_value() ||
        !(hover.value().context == request_context()))
    {
        return std::nullopt;
    }
    return core::hover_tip_text(hover.value().text.value());
}

// 届いた答えを使う（答えが届く合図の道の中・receive_lsp から）。kind は答えた要求の種類で、
// その種類の答えだけを見る。寿命の要求と補完（段 B）はここでは何もしない。
void EditorController::settle_language_answers(LspRequestKind kind)
{
    switch (kind)
    {
    case LspRequestKind::initialize:
    case LspRequestKind::shutdown:
    case LspRequestKind::completion:
        return;
    case LspRequestKind::hover:
        settle_hover_answer();
        return;
    case LspRequestKind::definition:
        settle_definition_answer();
        return;
    }
}

// 入力行・面・右クリックのメニュー・IME の変換が開いていない。
bool EditorController::faces_closed() const noexcept
{
    return !state_.command_input().has_value() && !context_menu_active() &&
           !state_.composition().has_value();
}

// 問いを出したときと同じ「何も待っていない」状態か。Vim は NORMAL で、オペレータ・回数・レジスタの
// 指定・次の鍵の待ち（`g` `f` `"` `m` …）が無い。通常モードは選択の起点が問いのときのまま
// （キャレットは答えの文脈が照合済み）。
bool EditorController::awaits_nothing() const noexcept
{
    switch (state_.mode())
    {
    case core::EditMode::ordinary:
        return !definition_anchor_.has_value() ||
               definition_anchor_.value() == state_.selection().anchor;
    case core::EditMode::vim:
        break;
    }
    const core::VimState &vim = state_.vim();
    return vim.mode == core::VimMode::normal && !vim.count.has_value() &&
           !vim.pending.has_value() && !vim.input_wait.has_value() &&
           !vim.selected_register.has_value() && !vim.register_count.has_value();
}

// hover の答え（決定 6）。入力行・面・メニュー・変換が開いている間と、窓が非アクティブの間に
// 届いた答えは札にせずに捨てる（知らせない・#1843 のレビュー R2 / R3）。空の答えと、切り詰めたら
// 文が残らない答え（空白だけ・R6）は札を出さずに 1 行知らせて消す。
void EditorController::settle_hover_answer()
{
    const auto &hover = state_.language_answers().hover;
    if (!hover.has_value())
    {
        return;
    }
    if (!faces_closed() || window_inactive_)
    {
        drop_hover();
        return;
    }
    const auto &text = hover.value().text;
    if (text.has_value() && !core::hover_tip_text(text.value()).empty())
    {
        return;
    }
    drop_hover();
    state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_information_not_found));
}

// 定義の答え（決定 7）は 1 回使って消す。入力行・面・メニュー・変換が開いているか、問いを出した
// ときの「何も待っていない」状態でなければ跳ばずに捨てる（知らせない・#1843 のレビュー R1。
// `gd` の後に `d` を打ってから届いた答えで別の所へ跳ぶと、次の鍵がそこで本文を変える）。
void EditorController::settle_definition_answer()
{
    const auto &definition = state_.language_answers().definition;
    if (!definition.has_value())
    {
        return;
    }
    const std::vector<core::LspLocation> locations = definition.value().locations;
    LanguageAnswers answers = state_.language_answers();
    answers.definition.reset();
    state_ = state_.with_language_answers(std::move(answers));
    const bool usable = faces_closed() && awaits_nothing();
    definition_anchor_.reset();
    if (usable)
    {
        land_definition(locations);
    }
}

// 定義へ（決定 7）。最初の場所へ跳ぶ。同じ文書なら `''` を置いてキャレットを移し、別のファイルなら
// 開いてから着地する。どちらも全文検索の当たりと同じ着地の 1 本（land_project_hit）。場所が 2 つ
// 以上なら「1 / N」、無ければ・`file:` でなければ 1 行知らせる。
void EditorController::land_definition(const std::vector<core::LspLocation> &locations)
{
    if (locations.empty())
    {
        state_ =
            state_.with_command_message(ui_notice(core::UiMessage::notice_definition_not_found));
        return;
    }
    const auto file = core::file_path_of_uri(locations.front().uri);
    if (!file.has_value())
    {
        state_ =
            state_.with_command_message(ui_notice(core::UiMessage::notice_definition_not_a_file));
        return;
    }
    if (locations.size() > 1)
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_definition_first_of,
                                                       {std::to_string(locations.size())}));
    }
    project_landing_ = ProjectHitLanding{file.value(), locations.front().range.start};
    const auto &path = state_.document().path;
    if (!path.has_value() || !ports_.files.same_file(path.value(), file.value()))
    {
        open_listed(file.value(), std::nullopt, OpenedAs::pinned);
    }
    land_project_hit();
}
} // namespace nenenib::application
