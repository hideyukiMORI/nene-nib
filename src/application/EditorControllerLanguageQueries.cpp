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
#include "LspLanding.hpp"
#include "LspLocation.hpp"
#include "LspRequestKind.hpp"
#include "OpenedAs.hpp"
#include "ProjectHitLanding.hpp"
#include "QueryLanguage.hpp"
#include "ServerAbilities.hpp"
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

namespace
{
// Vim の INSERT で頼むと断る問いの 1 行（断らない問いは値なし）。整形（文書・選択範囲・決定 7 の
// 追補）と名前の変更（#2004・ADR 0194 の決定 11・答えは入力の途中で当たらない）。
[[nodiscard]] std::optional<core::UiMessage> insert_refusal(core::LanguageQueryKind kind) noexcept
{
    switch (kind)
    {
    case core::LanguageQueryKind::definition:
    case core::LanguageQueryKind::hover:
    case core::LanguageQueryKind::references:
    case core::LanguageQueryKind::symbols:
    case core::LanguageQueryKind::signature_help:
        return std::nullopt;
    case core::LanguageQueryKind::format_document:
    case core::LanguageQueryKind::format_selection:
        return core::UiMessage::notice_formatting_in_insert;
    case core::LanguageQueryKind::rename:
        return core::UiMessage::notice_rename_in_insert;
    // クイックフィックス（#2005・答えの edit は入力の途中で当たらない）。
    case core::LanguageQueryKind::code_action:
        return core::UiMessage::notice_code_action_in_insert;
    }
    std::unreachable();
}

// 頼む前に断る問いの 1 行。Vim の INSERT で insert_refusal の問いを頼んだときだけ。
[[nodiscard]] std::optional<core::UiMessage>
refuses_in_insert(core::LanguageQueryKind kind, core::EditMode mode, const core::VimState &vim)
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return std::nullopt;
    case core::EditMode::vim:
        break;
    }
    return vim.mode == core::VimMode::insert ? insert_refusal(kind) : std::nullopt;
}
} // namespace

// 操作「定義へ移動」（F12）と「情報を表示」。言語サーバーが動いていない文書（要求を出せない文書）
// では 1 行知らせる。
void EditorController::accept(const QueryLanguage &intent)
{
    // 設定の面が開いていた（この意図の入口で閉じた）なら、問いを出さない（面どうしの排他）。
    if (settings_closed_now_)
    {
        return;
    }
    // Vim の INSERT で頼んだ整形と名前の変更は、答えが届いても入力の途中で当たらないので、頼まずに
    // 1 行で断る（#1997 の 2 回目のレビュー F1・設計席の決め 2026-11-24・#2004）。
    if (const auto refusal = refuses_in_insert(intent.kind, state_.mode(), state_.vim()))
    {
        state_ = state_.with_command_message(ui_notice(refusal.value()));
        return;
    }
    if (query_language(intent.kind))
    {
        return;
    }
    // 参照と記号は、動いている言語サーバーが答えないことを分けて知らせる（ADR 0191 の決定 2）。
    state_ = state_.with_command_message(ui_notice(unanswered_notice(intent.kind)));
}

namespace
{
// 能力の欄と「〜に答えません」の 1 行の組（ADR 0191 の決定 2）。
using UnansweredAbility = std::pair<bool ServerAbilities::*, core::UiMessage>;

// 問いの種類 → 分けて知らせる能力（定義へと hover は分けない）。
[[nodiscard]] std::optional<UnansweredAbility> unanswered_ability(core::LanguageQueryKind kind)
{
    switch (kind)
    {
    case core::LanguageQueryKind::definition:
    case core::LanguageQueryKind::hover:
        return std::nullopt;
    case core::LanguageQueryKind::references:
        return UnansweredAbility{&ServerAbilities::references,
                                 core::UiMessage::notice_references_not_supported};
    case core::LanguageQueryKind::symbols:
        return UnansweredAbility{&ServerAbilities::document_symbols,
                                 core::UiMessage::notice_symbols_not_supported};
    case core::LanguageQueryKind::signature_help:
        return UnansweredAbility{&ServerAbilities::signature_help,
                                 core::UiMessage::notice_signature_not_supported};
    case core::LanguageQueryKind::format_document:
        return UnansweredAbility{&ServerAbilities::formatting,
                                 core::UiMessage::notice_formatting_not_supported};
    case core::LanguageQueryKind::format_selection:
        return UnansweredAbility{&ServerAbilities::range_formatting,
                                 core::UiMessage::notice_range_formatting_not_supported};
    case core::LanguageQueryKind::rename:
        return UnansweredAbility{&ServerAbilities::rename,
                                 core::UiMessage::notice_rename_not_supported};
    case core::LanguageQueryKind::code_action:
        return UnansweredAbility{&ServerAbilities::code_action,
                                 core::UiMessage::notice_code_action_not_supported};
    }
    std::unreachable();
}
} // namespace

// 問いを出せなかったときの 1 行（ADR 0191 の決定 2）。能力の無い席は「〜に答えません」。
core::UiMessage EditorController::unanswered_notice(core::LanguageQueryKind kind) const
{
    const auto ability = unanswered_ability(kind);
    if (ability.has_value() && lacks_ability(ability.value().first))
    {
        return ability.value().second;
    }
    return core::UiMessage::notice_language_server_not_running;
}

// `gd` `K` `grr` `gO`。出せなければ何もしない（engine
// の歩は表に行が無かったときと同じ失敗のまま）。 `grr` と `gO` は頼めたら stepped_vim
// が成功の歩に読み替える（診断の鍵と同じ・ADR 0191 の決定 4・ #1939 のレビュー R3 の推奨 3）。`gd`
// `K` は ADR 0190 の追補 4 のまま失敗の歩。
void EditorController::perform(const core::VimLanguageQuery &effect)
{
    const bool asked = query_language(effect.kind);
    switch (effect.kind)
    {
    case core::LanguageQueryKind::definition:
    case core::LanguageQueryKind::hover:
    case core::LanguageQueryKind::signature_help:
    case core::LanguageQueryKind::format_document:
    case core::LanguageQueryKind::format_selection:
        return;
    case core::LanguageQueryKind::references:
    case core::LanguageQueryKind::symbols:
    // `grn`（#2004・ADR 0194 の決定 7）と `gra`（#2005・決定 8）も `grr` と同じく頼めたら成功の歩。
    case core::LanguageQueryKind::rename:
    case core::LanguageQueryKind::code_action:
        vim_effect_landed_ = asked;
        return;
    }
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
    case core::LanguageQueryKind::references:
        definition_anchor_ = state_.selection().anchor;
        return request_references();
    case core::LanguageQueryKind::symbols:
        definition_anchor_ = state_.selection().anchor;
        return request_document_symbols();
    case core::LanguageQueryKind::signature_help:
        return invoke_signature_help();
    case core::LanguageQueryKind::format_document:
        return request_formatting(false);
    case core::LanguageQueryKind::format_selection:
        return request_formatting(true);
    case core::LanguageQueryKind::rename:
        return request_rename_target();
    case core::LanguageQueryKind::code_action:
        definition_anchor_ = state_.selection().anchor;
        return request_code_actions();
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
    case LspRequestKind::references:
        settle_references_answer();
        return;
    case LspRequestKind::document_symbol:
        settle_symbols_answer();
        return;
    case LspRequestKind::signature_help:
        settle_signature_answer();
        return;
    case LspRequestKind::formatting:
    case LspRequestKind::range_formatting:
        settle_formatting_answer();
        return;
    case LspRequestKind::prepare_rename:
        settle_rename_target_answer();
        return;
    case LspRequestKind::rename:
        settle_rename_answer();
        return;
    case LspRequestKind::code_action:
        settle_code_actions_answer();
        return;
    case LspRequestKind::code_action_resolve:
        settle_code_action_resolve_answer();
        return;
    case LspRequestKind::execute_command:
        return;
    }
}

// 入力行・面・右クリックのメニュー・IME の変換・補完の面（答えを待っていてまだ見えていない面を
// 含む）・設定の面が開いていない。設定の面が開いている間は hover
// の札を出さず、定義の答えで跳ばない。補完の面と hover
// の札は同時に出さず、補完の面が開いている間は定義の答えで 跳ばない（ADR 0190 の決定 5・6）。
bool EditorController::faces_closed() const noexcept
{
    return !state_.command_input().has_value() && !context_menu_active() &&
           !state_.composition().has_value() && !completion_open() && !settings_open();
}

// 問いを出したときと同じ「何も待っていない」状態か。Vim は NORMAL で、オペレータ・回数・レジスタの
// 指定・次の鍵の待ち（`g` `f` `"` `m` …）・`map` の待ちの鍵・印・`Ctrl-o`
// の待ちが無い。通常モードは選択の起点が問いのときのまま （キャレットは答えの文脈が照合済み）。
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
    return state_.vim().mode == core::VimMode::normal && !vim_waits_for_key();
}

bool EditorController::vim_waits_for_key() const noexcept
{
    switch (state_.mode())
    {
    case core::EditMode::ordinary:
        return false;
    case core::EditMode::vim:
        break;
    }
    const core::VimState &vim = state_.vim();
    // `map` の左辺の途中で積んだ鍵（engine に届いていない）・イージーモーションの印・INSERT の
    // `Ctrl-o` の後の 1 命令の待ちも「待っている」（#1843 の再確認 P1・L1）。
    return vim.count.has_value() || vim.pending.has_value() || vim.input_wait.has_value() ||
           vim.selected_register.has_value() || vim.register_count.has_value() ||
           vim.easy_motion.has_value() || vim.insert_restart.has_value() ||
           !state_.key_map_pending().empty();
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
    const LanguageDefinition answered = definition.value();
    LanguageAnswers answers = state_.language_answers();
    answers.definition.reset();
    state_ = state_.with_language_answers(std::move(answers));
    const bool usable = faces_closed() && awaits_nothing();
    definition_anchor_.reset();
    if (usable)
    {
        land_definition(answered);
    }
}

// 定義へ（決定 7）。最初の場所へ跳ぶ。同じ文書なら `''` を置いてキャレットを移し、別のファイルなら
// 開いてから着地する。どちらも全文検索の当たりと同じ着地の 1 本（land_project_hit）。場所が 2 つ
// 以上なら「1 / N」、無ければ・`file:` でなければ 1 行知らせる。着地の位置の単位は答えた席のもの
// （#1916 のレビュー R2）。
void EditorController::land_definition(const LanguageDefinition &definition)
{
    const auto &locations = definition.locations;
    if (locations.empty())
    {
        state_ =
            state_.with_command_message(ui_notice(core::UiMessage::notice_definition_not_found));
        return;
    }
    // 今の文書かの照合は診断の照合（same_document_uri）と同じ規則（file_path_of_uri と
    // same_file）。
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
    land_location(file.value(),
                  LspLanding{.at = locations.front().range.start, .encoding = definition.encoding});
}

// 定義と参照の跳び（ADR 0190 の決定 7・ADR 0191 の決定 4）。全文検索の当たりと同じ着地の 1 本。
void EditorController::land_location(const core::FilePath &file, const LspLanding &at)
{
    project_landing_ = ProjectHitLanding{file, at};
    const auto &path = state_.document().path;
    if (!path.has_value() || !ports_.files.same_file(path.value(), file))
    {
        open_listed(file, std::nullopt, OpenedAs::pinned);
    }
    land_project_hit();
}
} // namespace nenenib::application
