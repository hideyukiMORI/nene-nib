#pragma once

#include "CostTier.hpp"
#include "DisplayLanguage.hpp"
#include "EditorFeature.hpp"
#include "EditorSettings.hpp"
#include "FeatureCost.hpp"
#include "FeatureStorage.hpp"
#include "SettingsPreset.hpp"
#include "SpeedSummary.hpp"
#include "UiMessage.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
// 基準の 1 打鍵（µs）。eng/perf-reference.json の `key-to-frame-single` の採用値 0.641 ms を写す
// （ADR 0170 の決定 5・機械ごとの実測は読まない・「見込み」の基準）。
inline constexpr std::uint16_t reference_single_key_us = 641;

// 機能の表（ADR 0170 の決定 1・2・2026-11-18 の追補 1・3）。EditorFeature の宣言の順に 1 行ずつ。
// 測った数字（single_key_delta_us・bench）は docs/quality/feature-costs.md の行を写したもので、
// 文書の行と表の一致は試験 `--feature-costs` が見る。数字を変えるときは文書と同じ PR で変える。
inline constexpr std::array<FeatureCost, 14> feature_costs{{
    {EditorFeature::wrap, "wrap", true, FeatureStorage::own_key, 0, "", false,
     UiMessage::settings_wrap_name, UiMessage::settings_wrap_description},
    {EditorFeature::guide, "guide", true, FeatureStorage::own_key, 0, "", false,
     UiMessage::settings_guide_name, UiMessage::settings_guide_description},
    {EditorFeature::autoindent, "autoindent", false, FeatureStorage::own_key, 0, "", false,
     UiMessage::settings_autoindent_name, UiMessage::settings_autoindent_description},
    {EditorFeature::matchparen, "matchparen", true, FeatureStorage::always_on, 17,
     "matchparen-single-key", false, UiMessage::settings_matchparen_name,
     UiMessage::settings_matchparen_description},
    {EditorFeature::easymotion, "easymotion", true, FeatureStorage::always_on, 0, "", false,
     UiMessage::settings_easymotion_name, UiMessage::settings_easymotion_description},
    {EditorFeature::explorer, "explorer", false, FeatureStorage::own_key, 0, "", false,
     UiMessage::settings_explorer_name, UiMessage::settings_explorer_description},
    {EditorFeature::minimap, "minimap", false, FeatureStorage::listed, 0, "", true,
     UiMessage::settings_minimap_name, UiMessage::settings_minimap_description},
    {EditorFeature::start_screen, "start_screen", true, FeatureStorage::always_on, 0, "", false,
     UiMessage::settings_start_screen_name, UiMessage::settings_start_screen_description},
    {EditorFeature::lsp, "lsp", false, FeatureStorage::own_key, 200, "lsp-single-key", true,
     UiMessage::settings_lsp_name, UiMessage::settings_lsp_description},
    {EditorFeature::syntax, "syntax", true, FeatureStorage::per_document, 0, "", false,
     UiMessage::settings_syntax_name, UiMessage::settings_syntax_description},
    {EditorFeature::caret_motion, "caretmotion", false, FeatureStorage::own_key, 0, "", false,
     UiMessage::settings_caret_motion_name, UiMessage::settings_caret_motion_description},
    {EditorFeature::caret_sparks, "caretsparks", false, FeatureStorage::own_key, 0, "", false,
     UiMessage::settings_caret_sparks_name, UiMessage::settings_caret_sparks_description},
    {EditorFeature::indent_guides, "indentguides", false, FeatureStorage::listed, 0, "", false,
     UiMessage::settings_indent_guides_name, UiMessage::settings_indent_guides_description},
    {EditorFeature::list, "list", false, FeatureStorage::listed, 0, "", false,
     UiMessage::settings_list_name, UiMessage::settings_list_description},
}};

// 表の行が宣言の順に並んでいること（行が欠けた・入れ替わった表はここで落ちる）。
[[nodiscard]] consteval bool feature_costs_in_order() noexcept
{
    for (std::size_t index = 0; index < feature_costs.size(); ++index)
    {
        if (std::to_underlying(feature_costs.at(index).feature) != index)
        {
            return false;
        }
    }
    return std::to_underlying(EditorFeature::list) + 1U == feature_costs.size();
}
static_assert(feature_costs_in_order(), "feature_costs follows the EditorFeature declaration");

// 表の行（宣言の順で引く・ARC-001）。
[[nodiscard]] constexpr const FeatureCost &feature_cost(EditorFeature feature) noexcept
{
    return feature_costs.at(std::to_underlying(feature));
}

// 機能の名前（ASCII・`features=` と `:set` の名前）とその逆。表に無い名前は無し。
[[nodiscard]] std::string_view feature_name_of(EditorFeature feature) noexcept;
[[nodiscard]] std::optional<EditorFeature> feature_named(std::string_view name) noexcept;

// 段（決定 2）。delta_us が base_us の 1% 未満 none・5% 未満 low・20% 以下 medium・超えれば high。
// base_us が 0 なら high（割れない基準の上の増分は測れていないと読む）。
[[nodiscard]] CostTier tier_of(std::uint16_t delta_us, std::uint16_t base_us) noexcept;
// 表の行の段。測った行（bench が空でない）だけが値を持ち、background の行は high（追補 3）。
[[nodiscard]] std::optional<CostTier> feature_tier(EditorFeature feature) noexcept;

// 面のスイッチで切れるか（listed と own_key）。always_on と per_document は切れない。
[[nodiscard]] bool feature_switchable(EditorFeature feature) noexcept;
// 今の設定でオンか（追補 1）。listed は `features=` の名前の列、own_key は自分の鍵、always_on と
// per_document は表の既定。`features=` を読むのはこの 1 本と feature_set だけ。
[[nodiscard]] bool feature_enabled(const EditorSettings &settings, EditorFeature feature) noexcept;
// オン・オフを置いた設定。listed は名前と `no_<名前>` を外し、既定と違うときだけ末尾に書く（表に
// 無い名前とほかの名前の順は変えない）。切れない機能は設定をそのまま返す。
[[nodiscard]] EditorSettings feature_set(EditorSettings settings, EditorFeature feature, bool on);

// 組を当てた設定（決定 5・追補 3）。段の印のある切れる機能だけを組の規則で置き、ほかは変えない。
[[nodiscard]] EditorSettings preset_applied(EditorSettings settings, SettingsPreset preset);
// 要約（決定 5・追補 3）。表の和で O(機能の数)。
[[nodiscard]] SpeedSummary speed_summary(const EditorSettings &settings);

// 段の語（「小」「中」「大」・none は「なし」）。行の印と要約の内訳が同じ語を引く。
[[nodiscard]] UiMessage cost_tier_title(CostTier tier) noexcept;
// 要約の 1 行目（採用案の形・#1836 の差し戻し）。「いまの設定での 1 打鍵の見込み: 0.64 ms ＋ 大 × 1
// （約 0.86 ms）」。基準の 1 打鍵に、オンの印のある機能の段ごとの数（小 → 中 → 大・0
// の段は書かない） と、足した見込みを添える。何も足さないときは基準の「0.64 ms」だけ。ms は小数 2
// 桁の四捨五入。
[[nodiscard]] std::string speed_estimate_text(const SpeedSummary &summary,
                                              DisplayLanguage language);
} // namespace nenenib::core
