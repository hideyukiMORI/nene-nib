// 設定の面（ADR 0170 の決定 5・6・2026-11-18 の追補 2・3・施主決定 D102・#1016 の段 1）。開く・
// 閉じる・元に戻す・焦点の移動・切り替え・組・探す欄・節の札の群。項目の列は core の settings_rows
// の 1 つ、機能のオン・オフは feature_enabled / feature_set、要約は speed_summary。切り替えは
// その場で状態の設定に置き（adopt_settings・保存はしない）、ファイルへは閉じるときに 1 回だけ書く。
#include "EditorController.hpp"

#include "AdjustFontSize.hpp"
#include "CancelCommand.hpp"
#include "CancelComposition.hpp"
#include "ChangeSetting.hpp"
#include "ChooseSettingsChoice.hpp"
#include "ChooseSettingsPreset.hpp"
#include "CloseSettings.hpp"
#include "CloseSettingsChoice.hpp"
#include "DisplayLanguage.hpp"
#include "DisplayLanguageChoice.hpp"
#include "DisplayLanguageNames.hpp"
#include "EditorControllerDetail.hpp"
#include "EditorFeature.hpp"
#include "EditorSettings.hpp"
#include "FeatureCosts.hpp"
#include "FilterSettings.hpp"
#include "FocusSettingsRow.hpp"
#include "JumpSettingsSection.hpp"
#include "LanguageServerRowView.hpp"
#include "OpenSettings.hpp"
#include "OpenSettingsChoice.hpp"
#include "PointMinimap.hpp"
#include "PointSettingsChoice.hpp"
#include "ResolvedLanguage.hpp"
#include "RevertSettings.hpp"
#include "ScrollExplorer.hpp"
#include "ScrollLines.hpp"
#include "ScrollPane.hpp"
#include "ScrollPreviewTable.hpp"
#include "ScrollSettings.hpp"
#include "ScrollSettingsChoice.hpp"
#include "ScrollTabs.hpp"
#include "SettingsChoiceOpen.hpp"
#include "SettingsChoiceTimeout.hpp"
#include "SettingsChoiceView.hpp"
#include "SettingsChoices.hpp"
#include "SettingsControl.hpp"
#include "SettingsOpen.hpp"
#include "SettingsPanel.hpp"
#include "SettingsRowView.hpp"
#include "SettingsRows.hpp"
#include "SettingsSection.hpp"
#include "SettingsSectionTab.hpp"
#include "SettingsValue.hpp"
#include "SettingsView.hpp"
#include "StepSettings.hpp"
#include "ThemeChoice.hpp"
#include "TypeSettingsChoice.hpp"
#include "UiMessage.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
namespace
{
// 本文や面を送るだけで、文書もキャレットの置き場も選びも変えない意図（#1835 のレビュー R8・施主決定
// D102 の引き出しは本文を隠さないので、効き目を見るために本文を送っても面を閉じない）。本文の
// ホイール・もう 1 つの枠のホイール・ミニマップの押しとドラッグ・エクスプローラーのホイール・帯の
// ホイール・プレビューの表の横の送り（#1883）。Ctrl+ホイールの字の大きさも同じ（面の「文字の大きさ」の行がその場で変わる・効き目を
// 見ながら変える・#1836 の差し戻し）。本文のクリック・打鍵・編集・タブの切り替えはここに書かない
// （閉じて保存する）。
[[nodiscard]] bool only_scrolls(const EditorIntent &intent) noexcept
{
    return std::holds_alternative<ScrollLines>(intent) ||
           std::holds_alternative<AdjustFontSize>(intent) ||
           std::holds_alternative<ScrollPane>(intent) ||
           std::holds_alternative<ScrollPreviewTable>(intent) ||
           std::holds_alternative<PointMinimap>(intent) ||
           std::holds_alternative<ScrollExplorer>(intent) ||
           std::holds_alternative<ScrollTabs>(intent);
}

// 面を開いたまま受ける意図（面の意図・使う人の操作でない意図・送るだけの意図）。
[[nodiscard]] bool keeps_settings_open(const EditorIntent &intent) noexcept
{
    return detail::is_settings_intent(intent) || detail::not_user_operation(intent) ||
           only_scrolls(intent);
}

// 選ぶ欄の一覧を開いたまま受ける意図（#1926）。一覧の意図・焦点の行を見せる面の送りの追従
// （ScrollSettings・ui が意図ごとに寄せる）・使う人の操作でない意図（合図は一覧を閉じない）。ほかの
// 意図（面のほかの意図・本文の操作・タブの切り替え…）は写す前に一覧を決めずに閉じる。
[[nodiscard]] bool keeps_settings_choice_open(const EditorIntent &intent) noexcept
{
    return detail::is_settings_choice_intent(intent) ||
           std::holds_alternative<ScrollSettings>(intent) || detail::not_user_operation(intent);
}

// 表の行 key の値が選ぶ欄なら、その値（言語サーバーの節の行・スイッチ・段の上下・字体は値なし）。
[[nodiscard]] std::optional<core::SettingsValue> choice_value_of(std::size_t key) noexcept
{
    if (key >= core::settings_rows.size())
    {
        return std::nullopt;
    }
    const auto &subject = core::settings_rows.at(key).subject;
    if (core::settings_control_of(subject, false) != core::SettingsControl::choice)
    {
        return std::nullopt;
    }
    return std::get<core::SettingsValue>(subject);
}

// 節の札の 6 つ（宣言の順・配置と同じ core の 1 つ）。
constexpr auto &section_order = core::settings_sections;

// 表の行の表示値。
[[nodiscard]] SettingsRowView row_view(const core::SettingsRow &row,
                                       const core::EditorSettings &settings,
                                       core::DisplayLanguage language)
{
    SettingsRowView view{row,
                         core::settings_row_title(row.subject),
                         core::settings_row_description(row.subject),
                         core::settings_row_set_name(row.subject),
                         false,
                         false,
                         {},
                         std::nullopt,
                         false};
    // 選択肢を足すと else の枝が新しい型を値の関数に渡せずコンパイルが落ちる（CPP-002）。
    std::visit(
        [&view, &settings, language](auto subject)
        {
            if constexpr (std::is_same_v<decltype(subject), core::EditorFeature>)
            {
                view.on = core::feature_enabled(settings, subject);
                view.switchable = core::feature_switchable(subject);
                view.tier = core::feature_tier(subject);
                view.background = view.tier.has_value() && core::feature_cost(subject).background;
            }
            else
            {
                view.value = core::settings_value_text(settings, subject, language);
            }
        },
        row.subject);
    return view;
}

// 焦点の行の設定を変えた値（スイッチは切り替え・値は送る・切れない機能はそのまま）。
// 配色の送りは面を開いたときの配色を列に残す（kept・利用者のテーマへ戻れる・R5）。
[[nodiscard]] core::EditorSettings changed_settings(const core::SettingsRow &row,
                                                    core::EditorSettings settings, bool forward,
                                                    const std::optional<core::ThemeChoice> &kept)
{
    return std::visit(
        [&settings, forward, &kept](auto subject)
        {
            if constexpr (std::is_same_v<decltype(subject), core::EditorFeature>)
            {
                const bool on = core::feature_enabled(settings, subject);
                return core::feature_set(std::move(settings), subject, !on);
            }
            else
            {
                return core::settings_value_stepped(std::move(settings), subject, forward, kept);
            }
        },
        row.subject);
}
} // namespace

bool EditorController::settings_open() const noexcept
{
    return settings_.has_value();
}

// 開く前に、Ctrl+P の面・`:` の入力行（配色の仮当てごと）・変換・右クリックのメニューを閉じる
// （#1835 のレビュー R2・面と入力行を重ねない）。IME は閉じない: 面が開いている間は探す欄で
// 使え（ime_stance_in_settings）、面が閉じた所で窓が変換を取り消す（#1836 のレビュー M1）。
// メニューは入口の close_context_menu_before が既に閉じている。
void EditorController::accept(const OpenSettings &)
{
    if (settings_.has_value())
    {
        return;
    }
    if (state_.composition().has_value())
    {
        accept(CancelComposition{});
    }
    if (state_.command_input().has_value())
    {
        accept(CancelCommand{});
    }
    context_menu_.reset();
    // 補完の面と hover の札は設定の面と同時に出さない（入口でも閉じているが、ここで名指しする）。
    close_completion();
    drop_hover();
    settings_ = SettingsOpen{state_.settings(), {}, 0};
    // PC の中の言語サーバーを探すのは面を開いたときだけ（#1945・結果は合図の道で採る）。
    request_language_server_search();
}

void EditorController::accept(const CloseSettings &)
{
    close_settings();
}

void EditorController::accept(const RevertSettings &)
{
    if (settings_.has_value())
    {
        preview_settings(settings_.value().opened);
    }
}

void EditorController::accept(const StepSettings &intent)
{
    if (!settings_.has_value())
    {
        return;
    }
    const auto visible = visible_settings_rows();
    if (visible.empty())
    {
        return;
    }
    auto &focus = settings_.value().focus;
    std::size_t at = 0;
    while (at < visible.size() && visible.at(at) != focus)
    {
        ++at;
    }
    const std::size_t count = visible.size();
    if (at == count)
    {
        focus = visible.front();
        return;
    }
    focus = visible.at(intent.down ? (at + 1) % count : (at + count - 1) % count);
}

void EditorController::accept(const ChangeSetting &intent)
{
    const auto focused = focused_settings_row();
    if (!settings_.has_value() || !focused.has_value())
    {
        return;
    }
    // 「言語サーバー」の節の行は「使う」「外す」と畳みの 1 行（#1945）。
    if (focused.value() >= core::settings_rows.size())
    {
        act_on_language_server_row(focused.value() - core::settings_rows.size());
        return;
    }
    const auto &row = core::settings_rows.at(focused.value());
    auto changed =
        changed_settings(row, state_.settings(), intent.forward, settings_.value().opened.theme);
    // 切れない機能の行と字体の行は値が変わらないので、同期を通さない（レビュー L3）。
    if (!core::same_settings(changed, state_.settings()))
    {
        preview_settings(std::move(changed));
    }
}

void EditorController::accept(const ChooseSettingsPreset &intent)
{
    if (settings_.has_value())
    {
        preview_settings(core::preset_applied(state_.settings(), intent.preset));
    }
}

void EditorController::accept(const FilterSettings &intent)
{
    if (!settings_.has_value())
    {
        return;
    }
    settings_.value().query = intent.query;
    const auto visible = visible_settings_rows();
    auto &focus = settings_.value().focus;
    if (!visible.empty() && std::ranges::find(visible, focus) == visible.end())
    {
        focus = visible.front();
    }
}

void EditorController::accept(const JumpSettingsSection &intent)
{
    if (!settings_.has_value())
    {
        return;
    }
    for (const std::size_t index : visible_settings_rows())
    {
        if (settings_section_of(index) == intent.section)
        {
            settings_.value().focus = index;
            return;
        }
    }
}

// 見えている行の row 番目へ焦点を置く（行のクリック・Home / End・PageUp / PageDown・#1836）。
void EditorController::accept(const FocusSettingsRow &intent)
{
    if (!settings_.has_value())
    {
        return;
    }
    const auto visible = visible_settings_rows();
    if (!visible.empty())
    {
        settings_.value().focus = visible.at(std::min(intent.row, visible.size() - 1));
    }
}

// 行の領域の送り（ホイールと焦点の追従・#1836）。値は ui が配置から求める。
void EditorController::accept(const ScrollSettings &intent)
{
    if (settings_.has_value())
    {
        settings_.value().scroll = std::max(intent.top, 0);
    }
}

// 焦点の行の選ぶ欄の一覧を開く（#1926）。選ぼうとしている位置は今の値（列に無ければ先頭）で、見える
// 範囲の中ほどに来るように送る。
void EditorController::accept(const OpenSettingsChoice &)
{
    const auto focused = focused_settings_row();
    if (!settings_.has_value() || settings_.value().choice.has_value() || !focused.has_value())
    {
        return;
    }
    const auto value = choice_value_of(focused.value());
    if (!value.has_value())
    {
        return;
    }
    const auto &settings = state_.settings();
    const auto &kept = settings_.value().opened.theme;
    const auto count = core::settings_choice_count(settings, value.value(), kept);
    if (count == 0)
    {
        return;
    }
    const auto at = core::settings_choice_position(settings, value.value(), kept).value_or(0);
    settings_.value().choice =
        SettingsChoiceOpen{focused.value(), at, core::settings_choice_centred(count, at), {}};
}

void EditorController::accept(const PointSettingsChoice &intent)
{
    const auto count = settings_choice_count();
    if (count > 0 && settings_.has_value() && settings_.value().choice.has_value())
    {
        settings_.value().choice.value().candidate = std::min(intent.choice, count - 1);
    }
}

void EditorController::accept(const ScrollSettingsChoice &intent)
{
    const auto count = settings_choice_count();
    if (count > 0 && settings_.has_value() && settings_.value().choice.has_value())
    {
        settings_.value().choice.value().top = std::min(intent.top, count - 1);
    }
}

// 選ぼうとしている位置の値に決めて閉じる。決めた値は ChangeSetting と同じく preview_settings の
// 1 本でその場に効かせ（保存はしない）、閉じるときに書く（ADR 0170 の決定 5・6）。
void EditorController::accept(const ChooseSettingsChoice &)
{
    if (!settings_.has_value() || !settings_.value().choice.has_value())
    {
        return;
    }
    const auto open = settings_.value().choice.value();
    settings_.value().choice.reset();
    const auto value = choice_value_of(open.row);
    if (!value.has_value())
    {
        return;
    }
    auto chosen = core::settings_choice_applied(state_.settings(), value.value(), open.candidate,
                                                settings_.value().opened.theme);
    if (!core::same_settings(chosen, state_.settings()))
    {
        preview_settings(std::move(chosen));
    }
}

void EditorController::accept(const CloseSettingsChoice &)
{
    if (settings_.has_value())
    {
        settings_.value().choice.reset();
    }
}

// 頭文字の跳び。打った字の全体を置き、名前がそれで始まる最初の選択肢へ位置を置く（当たりが無ければ
// そのまま）。
void EditorController::accept(const TypeSettingsChoice &intent)
{
    if (!settings_.has_value() || !settings_.value().choice.has_value())
    {
        return;
    }
    auto &open = settings_.value().choice.value();
    const auto value = choice_value_of(open.row);
    if (!value.has_value())
    {
        return;
    }
    open.typed = intent.typed;
    const auto jumped = core::settings_choice_typed(
        core::settings_choice_texts(state_.settings(), value.value(),
                                    settings_.value().opened.theme, state_.display_language()),
        open.typed);
    if (jumped.has_value())
    {
        open.candidate = jumped.value();
    }
}

// 1 秒の合図: 打った字を忘れる（一覧と位置はそのまま）。
void EditorController::accept(const SettingsChoiceTimeout &)
{
    if (settings_.has_value() && settings_.value().choice.has_value())
    {
        settings_.value().choice.value().typed.clear();
    }
}

// 開いている一覧の選択肢の数（一覧が無ければ 0）。
std::size_t EditorController::settings_choice_count() const noexcept
{
    if (!settings_.has_value() || !settings_.value().choice.has_value())
    {
        return 0;
    }
    const auto value = choice_value_of(settings_.value().choice.value().row);
    return value.has_value() ? core::settings_choice_count(state_.settings(), value.value(),
                                                           settings_.value().opened.theme)
                             : 0;
}

// 入口の 1 か所（apply の begin_intent の後）。面が開いていて、面を開いたまま受ける意図でなければ、
// 写す前に閉じて保存する（窓を閉じる EndSession・タブの操作・本文のクリック・打鍵…・追補 2）。
// 選ぶ欄の一覧は、一覧を開いたまま受ける意図でなければ先に決めずに閉じる（#1926）。
void EditorController::close_settings_before(const EditorIntent &intent)
{
    // この意図の入口で書こうとして書けなかったか（同じ意図の中の書き直しを飛ばす・#1835 の再確認
    // R-b1）。
    settings_write_failed_now_ = false;
    settings_closed_now_ = false;
    if (settings_.has_value() && settings_.value().choice.has_value() &&
        !keeps_settings_choice_open(intent))
    {
        settings_.value().choice.reset();
    }
    if (settings_.has_value() && !keeps_settings_open(intent))
    {
        settings_closed_now_ = true;
        close_settings();
        settings_write_failed_now_ = settings_unsaved_;
    }
}

// 閉じて、開いたときの写しと違えば 1 回だけ書く（決定 6・追補 2）。書くのは write_settings の
// 1 本（persist_settings と同じ）。書けなければ状態の設定は変えたまま、1 行知らせて「まだ書けて
// いない設定がある」を覚え、窓を閉じるとき（EndSession）にもう 1 回だけ書く（#1835 のレビュー
// R1）。
void EditorController::close_settings()
{
    if (!settings_.has_value())
    {
        return;
    }
    const core::EditorSettings opened = std::move(settings_).value().opened;
    settings_.reset();
    if (core::same_settings(opened, state_.settings()) && !state_.settings_failure().has_value() &&
        !settings_unsaved_)
    {
        return;
    }
    settings_unsaved_ = !write_settings(state_.settings());
    if (settings_unsaved_)
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::settings_unsaved));
    }
}

// 窓を閉じるときの書き直し（R1）。面で変えた設定が書けていなければ 1 回だけ書く。2 回目も失敗
// すれば `:set` の失敗と同じく失敗を状態に置くだけ（write_settings）。
void EditorController::retry_unsaved_settings()
{
    if (!settings_unsaved_)
    {
        return;
    }
    settings_unsaved_ = false;
    // 同じ EndSession の入口で面を閉じて書けなかった直後は、間に何も変わっていないので試さない
    // （その 1 回が窓を閉じるときの 1 回・#1835 の再確認 R-b1）。
    if (settings_write_failed_now_)
    {
        return;
    }
    static_cast<void>(write_settings(state_.settings()));
}

// 面の切り替えをその場で効かせる（保存しない・追補 2）。persist_settings の書いた後の同期と同じ
// adopt_settings を通し、表示言語は `:set language=` と同じく解決し直し、言語サーバーをオンにした
// ときは `:set lsp` と同じく諦めた数を戻す（起動と停止は apply の出口の reconcile が行う）。
void EditorController::preview_settings(core::EditorSettings settings)
{
    const auto &before = state_.settings();
    const bool language_changed = settings.ui_language != before.ui_language;
    // 命令の行（「使う」「外す」「元に戻す」・#1945）が変わった言語は、写した後に席を合わせる。
    const auto commands_before = before.lsp_commands;
    if (settings.lsp && !before.lsp)
    {
        resume_language_servers();
    }
    // Vim が窓を決め直す option（`wrap`・#1524）は、`:set wrap` と同じく設定を写す前に今の
    // キャレットの決め直しを頼む（レビュー L1・follow_window_setting の 1 本）。
    if (settings.wrapping != before.wrapping)
    {
        follow_window_setting();
    }
    adopt_settings(std::move(settings));
    follow_language_server_commands(commands_before, state_.settings().lsp_commands);
    if (language_changed)
    {
        const auto choice = state_.settings().ui_language;
        const bool follows_windows = choice == core::DisplayLanguageChoice::system;
        state_ = state_.with_display_language(core::resolved_language(
            choice, follows_windows ? ports_.ui_language.observe() : std::nullopt));
    }
}

// 探す欄で絞った後に見えている行（core::settings_rows の添字・表の順）。
std::vector<std::size_t> EditorController::visible_settings_rows() const
{
    std::vector<std::size_t> visible;
    if (!settings_.has_value())
    {
        return visible;
    }
    const auto &query = settings_.value().query;
    for (std::size_t index = 0; index < core::settings_rows.size(); ++index)
    {
        if (core::settings_row_matches(core::settings_rows.at(index), query,
                                       state_.display_language()))
        {
            visible.push_back(index);
        }
    }
    // 「言語サーバー」の節は絞っていない間だけ（行の鍵は表の行の後ろ・畳んだ行があれば畳みの
    // 1 行が最後・#1945）。
    if (!query.empty())
    {
        return visible;
    }
    const auto rows = language_server_rows();
    const auto shown = shown_language_server_rows(rows);
    for (const std::size_t index : shown)
    {
        visible.push_back(core::settings_rows.size() + index);
    }
    if (shown.size() < rows.size())
    {
        visible.push_back(core::settings_rows.size() + rows.size());
    }
    return visible;
}

core::SettingsSection EditorController::settings_section_of(std::size_t key) const noexcept
{
    return key < core::settings_rows.size() ? core::settings_rows.at(key).section
                                            : core::SettingsSection::language_server;
}

// 焦点の行（見えている行のときだけ）。
std::optional<std::size_t> EditorController::focused_settings_row() const
{
    if (!settings_.has_value())
    {
        return std::nullopt;
    }
    const auto visible = visible_settings_rows();
    const std::size_t focus = settings_.value().focus;
    if (std::ranges::find(visible, focus) == visible.end())
    {
        return std::nullopt;
    }
    return focus;
}

// 開いている選ぶ欄の一覧の表示値（#1926・名前の列は見せるたびに core の列から作る）。
std::optional<SettingsChoiceView> EditorController::settings_choice_view() const
{
    if (!settings_.has_value() || !settings_.value().choice.has_value())
    {
        return std::nullopt;
    }
    const auto &open = settings_.value().choice.value();
    const auto value = choice_value_of(open.row);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    const auto &settings = state_.settings();
    const auto &kept = settings_.value().opened.theme;
    return SettingsChoiceView{
        core::settings_choice_texts(settings, value.value(), kept, state_.display_language()),
        core::settings_choice_position(settings, value.value(), kept), open.candidate, open.top,
        open.typed};
}

std::optional<SettingsView> EditorController::settings_view() const
{
    if (!settings_.has_value())
    {
        return std::nullopt;
    }
    const auto &settings = state_.settings();
    const auto visible = visible_settings_rows();
    const auto focused = focused_settings_row();
    SettingsView view{{},
                      std::nullopt,
                      {},
                      std::nullopt,
                      settings_.value().query,
                      core::speed_summary(settings),
                      {},
                      !core::same_settings(settings_.value().opened, settings),
                      settings_.value().scroll,
                      settings_.value().query.empty()};
    for (std::size_t tab = 0; tab < section_order.size(); ++tab)
    {
        const auto section = section_order.at(tab);
        view.tabs.at(tab) = SettingsSectionTab{
            section, core::settings_section_title(section),
            std::ranges::any_of(visible, [this, section](std::size_t index)
                                { return settings_section_of(index) == section; })};
    }
    const auto servers =
        view.lsp_listed ? language_server_rows() : std::vector<LanguageServerRowView>{};
    const std::size_t table = core::settings_rows.size();
    for (std::size_t position = 0; position < visible.size(); ++position)
    {
        const std::size_t index = visible.at(position);
        if (focused == std::optional<std::size_t>{index})
        {
            view.focused = position;
            view.section = settings_section_of(index);
        }
        if (index < table)
        {
            view.rows.push_back(
                row_view(core::settings_rows.at(index), settings, state_.display_language()));
        }
        else if (index - table < servers.size())
        {
            view.servers.push_back(servers.at(index - table));
        }
    }
    view.servers_hidden = servers.size() - view.servers.size();
    view.choice = settings_choice_view();
    return view;
}
} // namespace nenenib::application
