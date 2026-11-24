#pragma once

#include "DisplayText.hpp"
#include "EditorSettings.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExResult.hpp"

#include <expected>
#include <optional>
#include <span>
#include <string_view>

// `:set` の機能のスイッチとインデントガイドの形の option（ADR 0170 の 2026-11-18 の追補 1・
// ADR 0193 の決定 3・4・#1975）。読むのは ExResult.cpp の `:set` の読みだけ。
namespace nenenib::core::detail
{
// インデントガイドの形の option の名前（真偽の表・数の表・既定値の書き方が同じ名前を引く）。
inline constexpr std::string_view indent_guide_highlight_current_option_name =
    "indentguidehighlightcurrent";
inline constexpr std::string_view indent_guide_style_option_name = "indentguidestyle";
inline constexpr std::string_view indent_guide_color_count_option_name = "indentguidecolorcount";
inline constexpr std::string_view indent_guide_width_option_name = "indentguidewidth";

// `name=on` か `name=off` の文言。
[[nodiscard]] DisplayText switch_message(std::string_view name, bool on);
// 名前 `name`・`no<name>`・`<name>?` のどれかなら、その真偽か問い（値が無い）。
[[nodiscard]] std::optional<std::optional<bool>>
switch_request(std::string_view option, std::span<const std::string_view> names);

// `features=` の名前で持つ機能の `set <名前>` `set no<名前>` `set <名前>?`（表 feature_costs の
// listed の行から引く）。当たらなければ空。
[[nodiscard]] std::optional<ExResult> listed_feature_option(std::string_view option,
                                                            const EditorSettings &settings);
// インデントガイドの形の option（`indentguidehighlightcurrent` `indentguidestyle=`
// `indentguidecolorcount=` `indentguidewidth=`）。当たらなければ空。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
indent_guide_option(std::string_view option, const EditorSettings &settings);
} // namespace nenenib::core::detail
