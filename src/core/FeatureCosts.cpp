// 機能の表の読み口と `features=` の読み書きと要約（ADR 0170 の決定 1・2・5・2026-11-18 の
// 追補 1・3）。`features=` の名前の列を解釈するのはここの feature_enabled と feature_set の 2 本
// だけ（ARC-012・ミニマップの minimap_shown / minimap_set もここへ寄せた）。
#include "FeatureCosts.hpp"

#include "CostTier.hpp"
#include "DisplayLanguage.hpp"
#include "EditorFeature.hpp"
#include "EditorSettings.hpp"
#include "FeatureStorage.hpp"
#include "GuideVisibility.hpp"
#include "LineWrapping.hpp"
#include "SettingsPreset.hpp"
#include "SpeedSummary.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 既定オンの機能を切ったときに `features=` へ書く名前の接頭辞（追補 1）。
constexpr std::string_view off_prefix = "no_";

[[nodiscard]] std::string off_name_of(EditorFeature feature)
{
    std::string name(off_prefix);
    name += feature_name_of(feature);
    return name;
}

// `no_<name>` か（写しを作らずに比べる・feature_enabled は noexcept）。
[[nodiscard]] bool names_off(std::string_view entry, std::string_view name) noexcept
{
    return entry.size() == off_prefix.size() + name.size() && entry.starts_with(off_prefix) &&
           entry.substr(off_prefix.size()) == name;
}

[[nodiscard]] bool listed(const EditorSettings &settings, std::string_view name) noexcept
{
    return std::ranges::any_of(settings.features,
                               [name](std::string_view entry) { return entry == name; });
}

[[nodiscard]] bool listed_off(const EditorSettings &settings, std::string_view name) noexcept
{
    return std::ranges::any_of(settings.features,
                               [name](std::string_view entry) { return names_off(entry, name); });
}

// 自分の鍵を持つ機能の今の値（決定 3）。
[[nodiscard]] bool own_key_enabled(const EditorSettings &settings, EditorFeature feature) noexcept
{
    switch (feature)
    {
    case EditorFeature::wrap:
        return settings.wrapping == LineWrapping::wrap;
    case EditorFeature::guide:
        return settings.guide == GuideVisibility::shown;
    case EditorFeature::autoindent:
        return settings.autoindent;
    case EditorFeature::explorer:
        return settings.explorer;
    case EditorFeature::lsp:
        return settings.lsp;
    case EditorFeature::caret_motion:
        return settings.caret_motion;
    case EditorFeature::matchparen:
    case EditorFeature::easymotion:
    case EditorFeature::minimap:
    case EditorFeature::start_screen:
    case EditorFeature::syntax:
        return feature_cost(feature).default_on;
    }
    std::unreachable();
}

// 自分の鍵を持つ機能に値を置く（決定 3）。鍵を持たない機能は何もしない。
void own_key_set(EditorSettings &settings, EditorFeature feature, bool on) noexcept
{
    switch (feature)
    {
    case EditorFeature::wrap:
        settings.wrapping = on ? LineWrapping::wrap : LineWrapping::no_wrap;
        return;
    case EditorFeature::guide:
        settings.guide = on ? GuideVisibility::shown : GuideVisibility::hidden;
        return;
    case EditorFeature::autoindent:
        settings.autoindent = on;
        return;
    case EditorFeature::explorer:
        settings.explorer = on;
        return;
    case EditorFeature::lsp:
        settings.lsp = on;
        return;
    case EditorFeature::caret_motion:
        settings.caret_motion = on;
        return;
    case EditorFeature::matchparen:
    case EditorFeature::easymotion:
    case EditorFeature::minimap:
    case EditorFeature::start_screen:
    case EditorFeature::syntax:
        return;
    }
    std::unreachable();
}

// `features=` の名前の列に置く（名前と `no_<名前>` を外し、既定と違うときだけ末尾に書く）。
void listed_set(EditorSettings &settings, const FeatureCost &row, bool on)
{
    std::erase_if(settings.features, [&row](std::string_view name)
                  { return name == row.name || names_off(name, row.name); });
    if (on != row.default_on)
    {
        settings.features.push_back(on ? std::string(row.name) : off_name_of(row.feature));
    }
}

// 組の規則で機能をオンにするか（決定 5）。
[[nodiscard]] bool preset_wants(SettingsPreset preset, CostTier tier) noexcept
{
    switch (preset)
    {
    case SettingsPreset::speed_first:
        return tier == CostTier::none;
    case SettingsPreset::recommended:
        return tier <= CostTier::low;
    case SettingsPreset::all_on:
        return true;
    }
    std::unreachable();
}

// 組が触る機能（段の印があって切れる）が組の規則のとおりか。
[[nodiscard]] bool preset_holds(const EditorSettings &settings, SettingsPreset preset) noexcept
{
    return std::ranges::all_of(feature_costs,
                               [&settings, preset](const FeatureCost &row)
                               {
                                   const auto tier = feature_tier(row.feature);
                                   return !tier.has_value() || !feature_switchable(row.feature) ||
                                          feature_enabled(settings, row.feature) ==
                                              preset_wants(preset, tier.value());
                               });
}

// 今の設定が当たる組。いくつも当たるときは おすすめ → 速さ優先 → 全部オン の順で最初の 1 つ
// （印のある機能が少ないうちは組が重なる・既定の組を先に見せる）。
constexpr std::array<SettingsPreset, 3> preset_order{
    SettingsPreset::recommended, SettingsPreset::speed_first, SettingsPreset::all_on};

[[nodiscard]] std::optional<SettingsPreset> preset_of(const EditorSettings &settings) noexcept
{
    for (const SettingsPreset preset : preset_order)
    {
        if (preset_holds(settings, preset))
        {
            return preset;
        }
    }
    return std::nullopt;
}

void count_tier(SpeedSummary &summary, EditorFeature feature, CostTier tier)
{
    switch (tier)
    {
    case CostTier::none:
        return;
    case CostTier::low:
        ++summary.low;
        return;
    case CostTier::medium:
        ++summary.medium;
        break;
    case CostTier::high:
        ++summary.high;
        break;
    }
    summary.heavy.push_back(feature);
}

// µs を「0.64」の形（ms・小数 2 桁・四捨五入）にする。
[[nodiscard]] std::string milliseconds_of(std::uint32_t microseconds)
{
    constexpr std::uint32_t half_hundredth_us = 5;
    constexpr std::uint32_t hundredth_us = 10;
    constexpr std::uint32_t hundred = 100;
    constexpr std::uint32_t two_digits = 10;
    const std::uint32_t hundredths = (microseconds + half_hundredth_us) / hundredth_us;
    const std::uint32_t fraction = hundredths % hundred;
    return std::to_string(hundredths / hundred) + (fraction < two_digits ? ".0" : ".") +
           std::to_string(fraction);
}

// 内訳の 1 項（「 ＋ 大 × 1」）。数が 0 の段は空。
[[nodiscard]] std::string tier_term(CostTier tier, std::size_t count, DisplayLanguage language)
{
    if (count == 0)
    {
        return {};
    }
    return ui_text_filled(UiMessage::settings_summary_term, language,
                          {ui_text(cost_tier_title(tier), language), std::to_string(count)});
}
} // namespace

std::string_view feature_name_of(EditorFeature feature) noexcept
{
    return feature_cost(feature).name;
}

std::optional<EditorFeature> feature_named(std::string_view name) noexcept
{
    for (const FeatureCost &row : feature_costs)
    {
        if (row.name == name)
        {
            return row.feature;
        }
    }
    return std::nullopt;
}

CostTier tier_of(std::uint16_t delta_us, std::uint16_t base_us) noexcept
{
    constexpr std::uint32_t percent = 100;
    constexpr std::uint32_t low_limit = 5;
    constexpr std::uint32_t medium_limit = 20;
    const std::uint32_t scaled = std::uint32_t{delta_us} * percent;
    const std::uint32_t base = base_us;
    if (base == 0)
    {
        return CostTier::high;
    }
    if (scaled < base)
    {
        return CostTier::none;
    }
    if (scaled < base * low_limit)
    {
        return CostTier::low;
    }
    if (scaled <= base * medium_limit)
    {
        return CostTier::medium;
    }
    return CostTier::high;
}

std::optional<CostTier> feature_tier(EditorFeature feature) noexcept
{
    const FeatureCost &row = feature_cost(feature);
    if (row.bench.empty())
    {
        return std::nullopt;
    }
    if (row.background)
    {
        return CostTier::high;
    }
    return tier_of(row.single_key_delta_us, reference_single_key_us);
}

bool feature_switchable(EditorFeature feature) noexcept
{
    switch (feature_cost(feature).storage)
    {
    case FeatureStorage::listed:
    case FeatureStorage::own_key:
        return true;
    case FeatureStorage::always_on:
    case FeatureStorage::per_document:
        return false;
    }
    std::unreachable();
}

bool feature_enabled(const EditorSettings &settings, EditorFeature feature) noexcept
{
    const FeatureCost &row = feature_cost(feature);
    switch (row.storage)
    {
    case FeatureStorage::listed:
        return row.default_on ? !listed_off(settings, row.name) : listed(settings, row.name);
    case FeatureStorage::own_key:
        return own_key_enabled(settings, feature);
    case FeatureStorage::always_on:
    case FeatureStorage::per_document:
        return row.default_on;
    }
    std::unreachable();
}

EditorSettings feature_set(EditorSettings settings, EditorFeature feature, bool on)
{
    const FeatureCost &row = feature_cost(feature);
    switch (row.storage)
    {
    case FeatureStorage::listed:
        listed_set(settings, row, on);
        return settings;
    case FeatureStorage::own_key:
        own_key_set(settings, feature, on);
        return settings;
    case FeatureStorage::always_on:
    case FeatureStorage::per_document:
        return settings;
    }
    std::unreachable();
}

EditorSettings preset_applied(EditorSettings settings, SettingsPreset preset)
{
    for (const FeatureCost &row : feature_costs)
    {
        const auto tier = feature_tier(row.feature);
        if (tier.has_value() && feature_switchable(row.feature))
        {
            settings =
                feature_set(std::move(settings), row.feature, preset_wants(preset, tier.value()));
        }
    }
    return settings;
}

SpeedSummary speed_summary(const EditorSettings &settings)
{
    SpeedSummary summary{reference_single_key_us, 0, 0, 0, {}, preset_of(settings)};
    for (const FeatureCost &row : feature_costs)
    {
        const auto tier = feature_tier(row.feature);
        if (!tier.has_value() || !feature_enabled(settings, row.feature))
        {
            continue;
        }
        summary.estimate_us += row.single_key_delta_us;
        count_tier(summary, row.feature, tier.value());
    }
    return summary;
}

UiMessage cost_tier_title(CostTier tier) noexcept
{
    switch (tier)
    {
    case CostTier::none:
        return UiMessage::settings_tier_none;
    case CostTier::low:
        return UiMessage::settings_tier_low;
    case CostTier::medium:
        return UiMessage::settings_tier_medium;
    case CostTier::high:
        return UiMessage::settings_tier_high;
    }
    std::unreachable();
}

std::string speed_estimate_text(const SpeedSummary &summary, DisplayLanguage language)
{
    // 基準は数字と単位だけ（「0.64 ms」・単位は訳さない）。
    std::string shown = milliseconds_of(reference_single_key_us) + " ms";
    const std::string terms = tier_term(CostTier::low, summary.low, language) +
                              tier_term(CostTier::medium, summary.medium, language) +
                              tier_term(CostTier::high, summary.high, language);
    if (!terms.empty())
    {
        shown += terms;
        shown += ui_text_filled(UiMessage::settings_summary_total, language,
                                {milliseconds_of(summary.estimate_us)});
    }
    return ui_text_filled(UiMessage::settings_summary_estimate, language, {shown});
}
} // namespace nenenib::core
