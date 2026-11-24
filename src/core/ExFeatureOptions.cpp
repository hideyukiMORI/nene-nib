// `:set` の機能のスイッチとインデントガイドの形の option（ADR 0170 の 2026-11-18 の追補 1・
// ADR 0193 の決定 3・4・#1975）。ExResult.cpp の長さの上限（CNF-014）のため分けた。
#include "ExFeatureOptions.hpp"

#include "DisplayText.hpp"
#include "EditorSettings.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExFailure.hpp"
#include "ExResult.hpp"
#include "FeatureCost.hpp"
#include "FeatureCosts.hpp"
#include "FeatureStorage.hpp"
#include "IndentGuide.hpp"
#include "IndentGuideStyle.hpp"

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core::detail
{
namespace
{
// インデントガイドの形（ADR 0193 の決定 4・#1975）。`set indentguidehighlightcurrent`
// `noindentguidehighlightcurrent` `indentguidehighlightcurrent?`、`set
// indentguidestyle=rainbow|plain` `indentguidestyle?` `indentguidestyle` （文字列の option
// の名前だけは問い）、`set indentguidecolorcount=N`（3〜6）と `indentguidewidth=N` （2 / 4 /
// 8・数の問いと `+=` などは数の表 number_options が引く）。値の読みは core の IndentGuide.hpp の 1
// 本ずつで、範囲の外・表に無い値は E474 と同じ失敗。`indentguidewidth?` は設定
// の値を答える（推し当てた値を見せる口は作らない・依頼書の決め）。
constexpr std::array<std::string_view, 1> indent_guide_highlight_current_names{
    indent_guide_highlight_current_option_name};

[[nodiscard]] std::optional<ExResult>
indent_guide_highlight_current_option(std::string_view option, const EditorSettings &settings)
{
    const auto request = switch_request(option, indent_guide_highlight_current_names);
    if (!request.has_value())
    {
        return std::nullopt;
    }
    const std::string_view name = indent_guide_highlight_current_names.front();
    if (!request.value().has_value())
    {
        return ExResult{std::nullopt, std::nullopt, std::nullopt,
                        switch_message(name, settings.indent_guide_highlight_current)};
    }
    EditorSettings changed = settings;
    changed.indent_guide_highlight_current = request.value().value();
    const bool wanted = changed.indent_guide_highlight_current;
    return ExResult{std::move(changed), std::nullopt, std::nullopt, switch_message(name, wanted)};
}

[[nodiscard]] DisplayText indent_guide_style_message(IndentGuideStyle style)
{
    return DisplayText::parse(std::string(indent_guide_style_option_name) + "=" +
                              std::string(indent_guide_style_name(style)))
        .value();
}

[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
indent_guide_style_option(std::string_view option, const EditorSettings &settings)
{
    if (!option.starts_with(indent_guide_style_option_name))
    {
        return std::nullopt;
    }
    const std::string_view rest = option.substr(indent_guide_style_option_name.size());
    if (rest.empty() || rest == "?")
    {
        return ExResult{std::nullopt, std::nullopt, std::nullopt,
                        indent_guide_style_message(settings.indent_guide_style)};
    }
    if (!rest.starts_with('='))
    {
        return std::nullopt;
    }
    const auto wanted = indent_guide_style_named(rest.substr(1));
    if (!wanted.has_value())
    {
        return std::unexpected(ExFailure::invalid_argument);
    }
    EditorSettings changed = settings;
    changed.indent_guide_style = wanted.value();
    return ExResult{std::move(changed), std::nullopt, std::nullopt,
                    indent_guide_style_message(wanted.value())};
}

// `name=` で始まるか。
[[nodiscard]] bool assigns(std::string_view option, std::string_view name) noexcept
{
    return option.starts_with(name) && option.substr(name.size()).starts_with('=');
}

// `indentguidecolorcount=N` と `indentguidewidth=N` の値を置く（問いは数の表が答える）。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
indent_guide_number_option(std::string_view option, const EditorSettings &settings)
{
    const bool colors = assigns(option, indent_guide_color_count_option_name);
    if (!colors && !assigns(option, indent_guide_width_option_name))
    {
        return std::nullopt;
    }
    const std::string_view value = option.substr(option.find('=') + 1);
    const auto parsed = colors ? indent_guide_color_count_of(value) : indent_guide_width_of(value);
    if (!parsed.has_value())
    {
        return std::unexpected(ExFailure::invalid_argument);
    }
    EditorSettings changed = settings;
    if (colors)
    {
        changed.indent_guide_color_count = parsed.value();
    }
    else
    {
        changed.indent_guide_width = parsed.value();
    }
    return ExResult{std::move(changed), std::nullopt, std::nullopt,
                    DisplayText::parse(option).value()};
}

} // namespace

[[nodiscard]] DisplayText switch_message(std::string_view name, bool on)
{
    return DisplayText::parse(std::string(name) + (on ? "=on" : "=off")).value();
}

// 名前 `name`・`no<name>`・`<name>?` のどれかなら、その真偽か問い（値が無い）。
[[nodiscard]] std::optional<std::optional<bool>>
switch_request(std::string_view option, std::span<const std::string_view> names)
{
    for (const std::string_view name : names)
    {
        if (option == name)
        {
            return std::optional<bool>{true};
        }
        if (option.starts_with("no") && option.substr(2) == name)
        {
            return std::optional<bool>{false};
        }
        if (option.ends_with('?') && option.substr(0, option.size() - 1) == name)
        {
            return std::optional<bool>{std::nullopt};
        }
    }
    return std::nullopt;
}

// `features=` の名前で持つ機能（ADR 0170 の 2026-11-18 の追補 1・ミニマップ ADR 0171 の決定 1・
// インデントガイド ADR 0193 の決定 3）。`set <名前>` `set no<名前>` `set <名前>?` は表
// feature_costs の listed の行から引き、名前の別表を書かない。文言は `minimap=on` の形（Vim
// に無い名前なので短い 名前は無い）。表の listed の行に当たらなければ空。
[[nodiscard]] std::optional<ExResult> listed_feature_option(std::string_view option,
                                                            const EditorSettings &settings)
{
    for (const FeatureCost &row : feature_costs)
    {
        const std::array<std::string_view, 1> names{row.name};
        const auto request =
            row.storage == FeatureStorage::listed ? switch_request(option, names) : std::nullopt;
        if (!request.has_value())
        {
            continue;
        }
        if (!request.value().has_value())
        {
            return ExResult{std::nullopt, std::nullopt, std::nullopt,
                            switch_message(row.name, feature_enabled(settings, row.feature))};
        }
        const bool wanted = request.value().value();
        return ExResult{feature_set(settings, row.feature, wanted), std::nullopt, std::nullopt,
                        switch_message(row.name, wanted)};
    }
    return std::nullopt;
}

// インデントガイドの形の option のどれか。どれにも当たらなければ空。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
indent_guide_option(std::string_view option, const EditorSettings &settings)
{
    auto active = indent_guide_highlight_current_option(option, settings);
    if (active.has_value())
    {
        return std::expected<ExResult, ExEvaluationFailure>{std::move(active).value()};
    }
    auto style = indent_guide_style_option(option, settings);
    if (style.has_value())
    {
        return style;
    }
    return indent_guide_number_option(option, settings);
}

} // namespace nenenib::core::detail
