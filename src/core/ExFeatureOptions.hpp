#pragma once

#include "DisplayText.hpp"
#include "EditorSettings.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExFailure.hpp"
#include "ExResult.hpp"

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// `:set` の機能のスイッチとインデントガイド・不可視文字の形の option（ADR 0170 の 2026-11-18 の
// 追補 1・ADR 0193 の決定 3・4・#1975・#1977）。読むのは ExResult.cpp の `:set` の読みだけ。
namespace nenenib::core::detail
{
// インデントガイドの形の option の名前（真偽の表・数の表・既定値の書き方が同じ名前を引く）。
inline constexpr std::string_view indent_guide_highlight_current_option_name =
    "indentguidehighlightcurrent";
inline constexpr std::string_view indent_guide_style_option_name = "indentguidestyle";
inline constexpr std::string_view indent_guide_color_count_option_name = "indentguidecolorcount";
inline constexpr std::string_view indent_guide_width_option_name = "indentguidewidth";
// 不可視文字の種類の option の名前（settings.v7 の鍵から下線を落とした形・ADR 0193 の決定 4）。
inline constexpr std::string_view list_eol_option_name = "listeol";
inline constexpr std::string_view list_tab_option_name = "listtab";
inline constexpr std::string_view list_wide_space_option_name = "listwidespace";
inline constexpr std::string_view list_space_option_name = "listspace";

// `name=on` か `name=off` の文言。
[[nodiscard]] DisplayText switch_message(std::string_view name, bool on);
// 名前 `name`・`no<name>`・`<name>?` のどれかなら、その真偽か問い（値が無い）。
[[nodiscard]] std::optional<std::optional<bool>>
switch_request(std::string_view option, std::span<const std::string_view> names);

// 名前の表から選ぶ文字列の option（`finderpreview` `indentguidestyle` `listspace`）の 1 本。
// `set <name>=<値>`・`set <name>?`・`set <name>`（文字列の option の名前だけは問い・Vim と同じ）。
// 文言は `<name>=<値の名前>`。表に無い値は E474 と同じ失敗、名前に当たらなければ空。Field は設定の
// 欄、Named は名前 → 値（表に無ければ値なし）、NameOf は値 → 名前。
template <auto Field, auto Named, auto NameOf>
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
chosen_option(std::string_view option, std::string_view name, const EditorSettings &settings)
{
    using Value = std::remove_cvref_t<decltype(settings.*Field)>;
    if (!option.starts_with(name))
    {
        return std::nullopt;
    }
    const std::string_view rest = option.substr(name.size());
    const auto message = [name](Value value)
    { return DisplayText::parse(std::string(name) + "=" + std::string(NameOf(value))).value(); };
    if (rest.empty() || rest == "?")
    {
        return ExResult{std::nullopt, std::nullopt, std::nullopt, message(settings.*Field)};
    }
    if (!rest.starts_with('='))
    {
        return std::nullopt;
    }
    const auto wanted = Named(rest.substr(1));
    if (!wanted.has_value())
    {
        return std::unexpected(ExFailure::invalid_argument);
    }
    EditorSettings changed = settings;
    changed.*Field = wanted.value();
    return ExResult{std::move(changed), std::nullopt, std::nullopt, message(wanted.value())};
}

// `features=` の名前で持つ機能の `set <名前>` `set no<名前>` `set <名前>?`（表 feature_costs の
// listed の行から引く）。当たらなければ空。
[[nodiscard]] std::optional<ExResult> listed_feature_option(std::string_view option,
                                                            const EditorSettings &settings);
// インデントガイドの形の option（`indentguidehighlightcurrent` `indentguidestyle=`
// `indentguidecolorcount=` `indentguidewidth=`）。当たらなければ空。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
indent_guide_option(std::string_view option, const EditorSettings &settings);
// 不可視文字の種類の option（`listeol` `listtab` `listwidespace` の真偽と `listspace=`・ADR 0193
// の決定 4）。全体の `list` は listed_feature_option が引く。当たらなければ空。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
list_option(std::string_view option, const EditorSettings &settings);
} // namespace nenenib::core::detail
