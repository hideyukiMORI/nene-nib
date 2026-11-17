// 設定の面（ADR 0170 の決定 5・6・2026-11-18 の追補 2・3・施主決定 D102・#1016 の段 1）。開く・
// 閉じる・元に戻す・焦点の移動・切り替え・組・探す欄・節の札の群。項目の列は core の settings_rows
// の 1 つ、機能のオン・オフは feature_enabled / feature_set、要約は speed_summary。切り替えは
// その場で状態の設定に置き（adopt_settings・保存はしない）、ファイルへは閉じるときに 1 回だけ書く。
#include "EditorController.hpp"

#include "ChangeSetting.hpp"
#include "ChooseSettingsPreset.hpp"
#include "CloseSettings.hpp"
#include "DisplayLanguageChoice.hpp"
#include "DisplayLanguageNames.hpp"
#include "EditorControllerDetail.hpp"
#include "EditorFeature.hpp"
#include "EditorSettings.hpp"
#include "FeatureCosts.hpp"
#include "FilterSettings.hpp"
#include "JumpSettingsSection.hpp"
#include "OpenSettings.hpp"
#include "ResolvedLanguage.hpp"
#include "RevertSettings.hpp"
#include "SettingsOpen.hpp"
#include "SettingsRowView.hpp"
#include "SettingsRows.hpp"
#include "SettingsSection.hpp"
#include "SettingsSectionTab.hpp"
#include "SettingsValue.hpp"
#include "SettingsView.hpp"
#include "StepSettings.hpp"
#include "UiMessage.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
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
// 面の意図（開いたまま受ける側）。
[[nodiscard]] bool is_settings_intent(const EditorIntent &intent) noexcept
{
    return std::holds_alternative<OpenSettings>(intent) ||
           std::holds_alternative<CloseSettings>(intent) ||
           std::holds_alternative<RevertSettings>(intent) ||
           std::holds_alternative<StepSettings>(intent) ||
           std::holds_alternative<ChangeSetting>(intent) ||
           std::holds_alternative<ChooseSettingsPreset>(intent) ||
           std::holds_alternative<FilterSettings>(intent) ||
           std::holds_alternative<JumpSettingsSection>(intent);
}

// 節の札の 6 つ（宣言の順）。
constexpr std::array<core::SettingsSection, 6> section_order{
    core::SettingsSection::display,  core::SettingsSection::text,
    core::SettingsSection::editing,  core::SettingsSection::panes,
    core::SettingsSection::features, core::SettingsSection::language_server};

// 表の行の表示値。
[[nodiscard]] SettingsRowView row_view(const core::SettingsRow &row,
                                       const core::EditorSettings &settings)
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
    if (std::holds_alternative<core::EditorFeature>(row.subject))
    {
        const auto feature = std::get<core::EditorFeature>(row.subject);
        view.on = core::feature_enabled(settings, feature);
        view.switchable = core::feature_switchable(feature);
        view.tier = core::feature_tier(feature);
        view.background = view.tier.has_value() && core::feature_cost(feature).background;
        return view;
    }
    view.value = core::settings_value_text(settings, std::get<core::SettingsValue>(row.subject));
    return view;
}

// 焦点の行の設定を変えた値（スイッチは切り替え・値は送る・切れない機能はそのまま）。
[[nodiscard]] core::EditorSettings changed_settings(const core::SettingsRow &row,
                                                    core::EditorSettings settings, bool forward)
{
    if (std::holds_alternative<core::EditorFeature>(row.subject))
    {
        const auto feature = std::get<core::EditorFeature>(row.subject);
        const bool on = core::feature_enabled(settings, feature);
        return core::feature_set(std::move(settings), feature, !on);
    }
    return core::settings_value_stepped(std::move(settings),
                                        std::get<core::SettingsValue>(row.subject), forward);
}
} // namespace

bool EditorController::settings_open() const noexcept
{
    return settings_.has_value();
}

void EditorController::accept(const OpenSettings &)
{
    if (settings_.has_value())
    {
        return;
    }
    settings_ = SettingsOpen{state_.settings(), {}, 0};
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
    if (!focused.has_value())
    {
        return;
    }
    const auto &row = core::settings_rows.at(focused.value());
    preview_settings(changed_settings(row, state_.settings(), intent.forward));
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
        if (core::settings_rows.at(index).section == intent.section)
        {
            settings_.value().focus = index;
            return;
        }
    }
}

// 入口の 1 か所（apply の begin_intent の後）。面が開いていて、意図が面の意図でも使う人の操作で
// ない意図でもなければ、写す前に閉じて保存する（窓を閉じる EndSession・タブの操作・本文の
// クリック…・追補 2）。
void EditorController::close_settings_before(const EditorIntent &intent)
{
    if (settings_.has_value() && !is_settings_intent(intent) && !detail::not_user_operation(intent))
    {
        close_settings();
    }
}

// 閉じて、開いたときの写しと違えば 1 回だけ書く（決定 6・追補 2）。書けなければ状態の設定は
// 変えたまま、失敗を状態に置いて 1 行知らせる（ADR 0114 の規則）。
void EditorController::close_settings()
{
    if (!settings_.has_value())
    {
        return;
    }
    const core::EditorSettings opened = std::move(settings_).value().opened;
    settings_.reset();
    if (core::same_settings(opened, state_.settings()) && !state_.settings_failure().has_value())
    {
        return;
    }
    const auto saved = ports_.settings.write(state_.settings());
    if (!saved)
    {
        state_ = state_.with_settings_failure(saved.error())
                     .with_command_message(ui_notice(core::UiMessage::settings_unsaved));
        return;
    }
    state_ = state_.with_settings_failure(std::nullopt);
}

// 面の切り替えをその場で効かせる（保存しない・追補 2）。persist_settings の書いた後の同期と同じ
// adopt_settings を通し、表示言語は `:set language=` と同じく解決し直し、言語サーバーをオンにした
// ときは `:set lsp` と同じく諦めた数を戻す（起動と停止は apply の出口の reconcile が行う）。
void EditorController::preview_settings(core::EditorSettings settings)
{
    const auto &before = state_.settings();
    const bool language_changed = settings.ui_language != before.ui_language;
    if (settings.lsp && !before.lsp)
    {
        language_server_.failures = 0;
        language_server_.given_up = false;
        language_server_.considered.reset();
    }
    adopt_settings(std::move(settings));
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
    return visible;
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
                      settings.lsp_commands,
                      !core::same_settings(settings_.value().opened, settings)};
    for (std::size_t tab = 0; tab < section_order.size(); ++tab)
    {
        const auto section = section_order.at(tab);
        view.tabs.at(tab) = SettingsSectionTab{
            section, core::settings_section_title(section),
            std::ranges::any_of(visible, [section](std::size_t index)
                                { return core::settings_rows.at(index).section == section; })};
    }
    for (const std::size_t index : visible)
    {
        if (focused == std::optional<std::size_t>{index})
        {
            view.focused = view.rows.size();
            view.section = core::settings_rows.at(index).section;
        }
        view.rows.push_back(row_view(core::settings_rows.at(index), settings));
    }
    return view;
}
} // namespace nenenib::application
