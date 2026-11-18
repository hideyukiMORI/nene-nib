#include "EditorController.hpp"

#include "EditorControllerDetail.hpp"
#include "EditorIntent.hpp"
#include "FileUri.hpp"
#include "HoverTip.hpp"
#include "LanguageAnswers.hpp"
#include "LanguageDefinition.hpp"
#include "LanguageHover.hpp"
#include "LanguageQueryKind.hpp"
#include "LspLocation.hpp"
#include "OpenedAs.hpp"
#include "ProjectHitLanding.hpp"
#include "QueryLanguage.hpp"
#include "UiMessage.hpp"
#include "VimLanguageQuery.hpp"
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
    repaint_ = state_.language_answers().hover.has_value();
    drop_hover();
}

// 札を消す意図を決める 1 か所（決定 6）。使う人の操作（鍵・クリック・ホイール・タブの切り替え…・
// not_user_operation でない意図）は写す前に消す。合図・窓の寸法・帯の hover は消さない（窓が
// 非アクティブになった知らせは accept が消す）。答えが無ければ欄を 1 つ読むだけ。
void EditorController::settle_hover(const EditorIntent &intent)
{
    if (state_.language_answers().hover.has_value() && !detail::not_user_operation(intent))
    {
        drop_hover();
    }
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

// 届いた答えを使う（答えが届く合図の道の中・receive_lsp から）。空の hover は札を出さずに 1 行
// 知らせて消す。定義の答えは 1 回使って消す。入力行か面が開いていれば跳ばない（答えは捨てる）。
void EditorController::settle_language_answers()
{
    const LanguageAnswers &held = state_.language_answers();
    if (held.hover.has_value() && !held.hover.value().text.has_value())
    {
        drop_hover();
        state_ =
            state_.with_command_message(ui_notice(core::UiMessage::notice_information_not_found));
    }
    const auto &definition = state_.language_answers().definition;
    if (!definition.has_value())
    {
        return;
    }
    const std::vector<core::LspLocation> locations = definition.value().locations;
    LanguageAnswers answers = state_.language_answers();
    answers.definition.reset();
    state_ = state_.with_language_answers(std::move(answers));
    if (!state_.command_input().has_value())
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
