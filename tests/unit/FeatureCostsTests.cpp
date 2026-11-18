// scope `--feature-costs` の単体テスト（ADR 0170 の決定 1・2・5・2026-11-18 の追補 1・3・#1828）。
// 機能の表の網羅と名前の往復・tier_of の境・`features=` の読み書き（既定オフの名前・既定オンの
// `no_<名前>`・表に無い名前を残す・ミニマップの寄せ）・要約と組・設定の面の項目の列と探す欄と値の
// 送り・切れる機能の `:set` の名前。時計・ファイル・乱数の種を読まない。
#include "Appearance.hpp"
#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "CostTier.hpp"
#include "DisplayLanguage.hpp"
#include "DisplayLanguageChoice.hpp"
#include "EditorFeature.hpp"
#include "EditorSettings.hpp"
#include "ExResult.hpp"
#include "FeatureCost.hpp"
#include "FeatureCosts.hpp"
#include "FeatureStorage.hpp"
#include "FinderPreview.hpp"
#include "GuideVisibility.hpp"
#include "LineWrapping.hpp"
#include "Minimap.hpp"
#include "Scopes.hpp"
#include "SettingsPreset.hpp"
#include "SettingsRow.hpp"
#include "SettingsRows.hpp"
#include "SettingsSection.hpp"
#include "SettingsValue.hpp"
#include "SpeedSummary.hpp"
#include "TestSupport.hpp"
#include "ThemeChoice.hpp"
#include "ThemeDocument.hpp"
#include "ThemeName.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
using core::EditorFeature;

// 表は宣言の順に全部の機能を持ち、名前は ASCII で往復し、既定の設定の値は表の既定と同じ。
void verify_table()
{
    const auto defaults = core::default_editor_settings();
    std::vector<std::string> names;
    for (const auto &row : core::feature_costs)
    {
        names.emplace_back(row.name);
        expect(core::feature_named(row.name) == std::optional{row.feature} &&
                   core::feature_name_of(row.feature) == row.name,
               "a feature name round-trips");
        expect(std::ranges::all_of(row.name,
                                   [](char c) { return (c >= 'a' && c <= 'z') || c == '_'; }),
               "a feature name is lower-case ASCII");
        expect(core::feature_enabled(defaults, row.feature) == row.default_on,
               "the default settings hold each feature's default");
        expect(row.bench.empty() == (row.single_key_delta_us == 0),
               "only a measured row carries a single-key delta");
    }
    std::ranges::sort(names);
    expect(std::ranges::adjacent_find(names) == names.end(), "feature names are unique");
    expect(!core::feature_named("no_matchparen").has_value() &&
               !core::feature_named("").has_value(),
           "names outside the table are not features");
}

// 段の境（決定 2）。基準 641 µs の 1% は 6.41、5% は 32.05、20% は 128.2。
void verify_tiers()
{
    using core::CostTier;
    constexpr std::uint16_t base = 641;
    expect(core::tier_of(0, base) == CostTier::none && core::tier_of(6, base) == CostTier::none,
           "under 1% is none");
    expect(core::tier_of(7, base) == CostTier::low && core::tier_of(32, base) == CostTier::low,
           "1% to under 5% is low");
    expect(core::tier_of(33, base) == CostTier::medium &&
               core::tier_of(128, base) == CostTier::medium,
           "5% to 20% is medium");
    expect(core::tier_of(129, base) == CostTier::high, "over 20% is high");
    expect(core::tier_of(100, 100) == CostTier::high &&
               core::tier_of(20, 100) == CostTier::medium &&
               core::tier_of(1, 100) == CostTier::low && core::tier_of(0, 0) == CostTier::high,
           "the bounds are exact and a zero base reads as high");
    expect(core::feature_tier(EditorFeature::matchparen) == std::optional{CostTier::low},
           "matchparen's measured 17 us is low");
    expect(core::feature_tier(EditorFeature::lsp) == std::optional{CostTier::high},
           "the language server runs a child process and is high");
    expect(!core::feature_tier(EditorFeature::minimap).has_value() &&
               !core::feature_tier(EditorFeature::wrap).has_value(),
           "an unmeasured feature has no tier, even when it works in the background");
}

// `features=` の読み書き（追補 1）。
void verify_features_list()
{
    const auto defaults = core::default_editor_settings();
    auto others = defaults;
    others.features = {"todo_marks2", "no_matchparen", "x"};
    const auto on = core::feature_set(others, EditorFeature::minimap, true);
    expect(on.features ==
                   std::vector<std::string>{"todo_marks2", "no_matchparen", "x", "minimap"} &&
               core::feature_enabled(on, EditorFeature::minimap),
           "a default-off feature is written by its name at the end, keeping unknown names");
    const auto off = core::feature_set(on, EditorFeature::minimap, false);
    expect(off.features == others.features && !core::feature_enabled(off, EditorFeature::minimap),
           "turning it off removes only its name");
    expect(core::feature_set(on, EditorFeature::minimap, true).features == on.features,
           "setting the same value writes the name once");
    auto stale = defaults;
    stale.features = {"no_minimap"};
    expect(!core::feature_enabled(stale, EditorFeature::minimap) &&
               core::feature_set(stale, EditorFeature::minimap, false).features.empty(),
           "a no_ name of a default-off feature means nothing and is cleaned when set");
    expect(core::feature_enabled(others, EditorFeature::matchparen),
           "a feature that cannot be turned off ignores its no_ name");
    expect(core::feature_set(others, EditorFeature::matchparen, false).features ==
                   others.features &&
               core::feature_set(others, EditorFeature::syntax, false).features == others.features,
           "features that cannot be switched leave the settings as they are");
    expect(core::minimap_shown(on) && !core::minimap_shown(off) &&
               core::minimap_set(others, true).features == on.features,
           "the minimap reads and writes through the feature table");
}

// 自分の鍵を持つ機能（決定 3）。
void verify_own_keys()
{
    const auto defaults = core::default_editor_settings();
    const auto wrap = core::feature_set(defaults, EditorFeature::wrap, false);
    const auto guide = core::feature_set(defaults, EditorFeature::guide, false);
    const auto indent = core::feature_set(defaults, EditorFeature::autoindent, true);
    const auto explorer = core::feature_set(defaults, EditorFeature::explorer, true);
    const auto lsp = core::feature_set(defaults, EditorFeature::lsp, true);
    expect(wrap.wrapping == core::LineWrapping::no_wrap &&
               guide.guide == core::GuideVisibility::hidden && indent.autoindent &&
               explorer.explorer && lsp.lsp,
           "own-key features write their own keys");
    expect(wrap.features.empty() && guide.features.empty() && indent.features.empty() &&
               explorer.features.empty() && lsp.features.empty(),
           "own-key features do not touch the features list");
    expect(!core::feature_enabled(wrap, EditorFeature::wrap) &&
               core::feature_enabled(lsp, EditorFeature::lsp),
           "own-key features read their own keys");
}

// 要約と組（決定 5・追補 3）。印のある機能は matchparen（low・切れない）と lsp（high）。
void verify_summary()
{
    const auto defaults = core::default_editor_settings();
    const auto quiet = core::speed_summary(defaults);
    expect(quiet.estimate_us == core::reference_single_key_us + 17 && quiet.low == 1 &&
               quiet.medium == 0 && quiet.high == 0 && quiet.heavy.empty(),
           "the default estimate adds only matchparen");
    expect(quiet.preset == std::optional{core::SettingsPreset::recommended},
           "the default settings are the recommended preset");
    auto with_lsp = core::feature_set(defaults, EditorFeature::lsp, true);
    with_lsp = core::feature_set(with_lsp, EditorFeature::minimap, true);
    const auto heavy = core::speed_summary(with_lsp);
    expect(heavy.estimate_us == core::reference_single_key_us + 17 + 200 && heavy.high == 1 &&
               heavy.heavy == std::vector<EditorFeature>{EditorFeature::lsp},
           "the language server adds its delta and is named as heavy; the unmeasured minimap is "
           "not counted");
    expect(heavy.preset == std::optional{core::SettingsPreset::all_on},
           "all marked features on is the all-on preset");
    const auto all = core::preset_applied(defaults, core::SettingsPreset::all_on);
    const auto speed = core::preset_applied(all, core::SettingsPreset::speed_first);
    const auto recommended = core::preset_applied(all, core::SettingsPreset::recommended);
    expect(all.lsp && !speed.lsp && !recommended.lsp, "presets switch only by the tier");
    expect(core::same_settings(speed, defaults) && core::same_settings(recommended, defaults),
           "presets touch only marked switchable features");
    auto outside = core::feature_set(defaults, EditorFeature::minimap, true);
    expect(core::speed_summary(outside).preset == std::optional{core::SettingsPreset::recommended},
           "an unmarked feature does not move the preset");
}

// 設定の面の項目の列（追補 2）。切れる機能と切れない機能は 1 回ずつ、per_document は載らない。
void verify_rows()
{
    for (const auto &cost : core::feature_costs)
    {
        const auto count =
            std::ranges::count_if(core::settings_rows, [&cost](const core::SettingsRow &row)
                                  { return row.subject == core::SettingsSubject{cost.feature}; });
        const bool shown = cost.storage != core::FeatureStorage::per_document &&
                           (core::feature_switchable(cost.feature) ||
                            core::feature_tier(cost.feature).has_value());
        expect(count == (shown ? 1 : 0),
               "a feature has one settings row when it can be switched or carries a tier");
    }
    expect(std::ranges::is_sorted(core::settings_rows, {}, [](const core::SettingsRow &row)
                                  { return std::to_underlying(row.section); }),
           "rows follow the section order");
    const auto lsp = core::SettingsRow{core::SettingsSection::language_server, EditorFeature::lsp};
    expect(core::settings_row_matches(lsp, "", core::DisplayLanguage::japanese) &&
               core::settings_row_matches(lsp, "LSP", core::DisplayLanguage::japanese) &&
               core::settings_row_matches(lsp, "言語サーバー", core::DisplayLanguage::japanese) &&
               core::settings_row_matches(lsp, "diagnostics", core::DisplayLanguage::english) &&
               !core::settings_row_matches(lsp, "minimap", core::DisplayLanguage::japanese),
           "the search box matches the :set name, the title and the description");
    const auto theme =
        core::SettingsRow{core::SettingsSection::display, core::SettingsValue::theme};
    expect(core::settings_row_matches(theme, "colorsch", core::DisplayLanguage::english),
           "a value row matches its :set name");
}

// 利用者のテーマ（一覧に無い配色）からでも送って戻れる（#1835 のレビュー R5）。
void verify_user_theme_steps()
{
    const auto &base = core::theme_of(core::BuiltinTheme::neutral_light);
    const auto user = core::ThemeChoice::from(
        core::ThemeDocument{core::ThemeName::parse("my-theme").value(),
                            base.appearance,
                            base.ui,
                            base.body,
                            {fixed_text("author"), fixed_text("MIT"), fixed_text("local")}});
    auto settings = core::default_editor_settings();
    settings.theme = user;
    const auto kept = std::optional{user};
    const auto forward =
        core::settings_value_stepped(settings, core::SettingsValue::theme, true, kept);
    expect(!forward.theme.has_value(), "the user theme is the last of the list: forward is system");
    const auto back =
        core::settings_value_stepped(forward, core::SettingsValue::theme, false, kept);
    expect(back.theme.has_value() && back.theme.value() == user,
           "stepping back from system returns to the kept user theme");
    const auto before =
        core::settings_value_stepped(settings, core::SettingsValue::theme, false, kept);
    const auto again = core::settings_value_stepped(before, core::SettingsValue::theme, true, kept);
    expect(before.theme.has_value() && !(before.theme.value() == user) && again.theme.has_value() &&
               again.theme.value() == user,
           "stepping back to the last built-in theme and forward again returns to the user theme");
    const auto plain = core::settings_value_stepped(
        core::default_editor_settings(), core::SettingsValue::theme, false, std::nullopt);
    expect(plain.theme.has_value() &&
               plain.theme.value() == core::ThemeChoice::from(static_cast<core::BuiltinTheme>(
                                          core::builtin_themes.size() - 1)),
           "without a user theme the list wraps from system to the last built-in theme");
}

// 値の送り（段 1 の値の行）。
void verify_values()
{
    const auto defaults = core::default_editor_settings();
    const auto themed =
        core::settings_value_stepped(defaults, core::SettingsValue::theme, true, std::nullopt);
    expect(themed.theme.has_value() &&
               core::settings_value_text(themed, core::SettingsValue::theme) != "system" &&
               core::settings_value_text(defaults, core::SettingsValue::theme) == "system",
           "the theme steps from system to the first built-in theme");
    expect(!core::settings_value_stepped(themed, core::SettingsValue::theme, false, std::nullopt)
                .theme.has_value(),
           "stepping back returns to system");
    expect(core::settings_value_stepped(defaults, core::SettingsValue::theme, false, std::nullopt)
               .theme.has_value(),
           "stepping back from system wraps to the last built-in theme");
    const auto bigger =
        core::settings_value_stepped(defaults, core::SettingsValue::font_size, true, std::nullopt);
    expect(core::settings_value_text(bigger, core::SettingsValue::font_size) == "14.5" &&
               core::settings_value_text(defaults, core::SettingsValue::font_size) == "13.5",
           "the font size steps by 1 pt");
    const auto language = core::settings_value_stepped(defaults, core::SettingsValue::ui_language,
                                                       true, std::nullopt);
    expect(defaults.ui_language == core::DisplayLanguageChoice::system &&
               language.ui_language == core::DisplayLanguageChoice::japanese,
           "the display language wraps through the listed choices");
    const auto preview = core::settings_value_stepped(defaults, core::SettingsValue::finder_preview,
                                                      false, std::nullopt);
    expect(preview.finder_preview == core::FinderPreview::off, "the finder preview wraps backward");
    expect(core::same_settings(core::settings_value_stepped(
                                   defaults, core::SettingsValue::font_family, true, std::nullopt),
                               defaults),
           "the font family is shown only in this stage");
    verify_user_theme_steps();
}

// 切れる機能の名前は `:set` が受ける名前（決定 4・生成の寄せは後の工程）。
void verify_set_names()
{
    const auto defaults = core::default_editor_settings();
    for (const auto &row : core::feature_costs)
    {
        if (!core::feature_switchable(row.feature))
        {
            continue;
        }
        const auto asked = core::evaluate_ex("set " + std::string(row.name) + "?", defaults,
                                             core::Appearance::dark);
        expect(asked.has_value(), "a switchable feature's name is a :set option");
    }
}
} // namespace

void verify_feature_costs_scope()
{
    verify_table();
    verify_tiers();
    verify_features_list();
    verify_own_keys();
    verify_summary();
    verify_rows();
    verify_values();
    verify_set_names();
}
} // namespace nenenib::tests
