#pragma once

#include "DisplayLanguage.hpp"
#include "DisplayLanguageChoice.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace nenenib::core
{
// 表示言語の名前の正本（ADR 0123 の改訂の決定 1・追補 2026-11-13 の決定 7・8）。希望値の ASCII の
// 名前（`system` `ja` `en` `zh-hans` `ko` `es` `de`）は設定 v4 の値と `:set language=` の値で同じ
// 1 つ。値を足すと switch がコンパイルで落ちる（CPP-002）。

// `:set language=` で選べる希望値（列のある 3 言語と system・面と補完の候補の順）。列の無い言語
// （ko es de）は設定ファイルでは読めて書けるが、ここには載せない（追補の決定 1）。
inline constexpr std::array<DisplayLanguageChoice, 4> listed_display_language_choices{
    DisplayLanguageChoice::japanese, DisplayLanguageChoice::english,
    DisplayLanguageChoice::simplified_chinese, DisplayLanguageChoice::system};

// 希望値の ASCII の名前。
[[nodiscard]] std::string_view display_language_choice_name(DisplayLanguageChoice choice) noexcept;

// 名前から希望値（7 値のどれか）。名前で無ければ値なし。
[[nodiscard]] std::optional<DisplayLanguageChoice>
display_language_choice_named(std::string_view name) noexcept;

// 解決済みの言語の ASCII の名前（`ja` `en` `zh-hans`・`:set language?` の括弧の中）。
[[nodiscard]] std::string_view display_language_code(DisplayLanguage language) noexcept;

// 言語の自称（`日本語` `English` `简体中文`）。どの表示言語でも同じ綴りで出す（追補の決定 7）。
[[nodiscard]] std::string_view display_language_endonym(DisplayLanguage language) noexcept;

// 候補の行に添える説明。system は表の「Windows に合わせる」（language の文）、ほかは自称。
[[nodiscard]] std::string_view display_language_choice_detail(DisplayLanguageChoice choice,
                                                              DisplayLanguage language) noexcept;
} // namespace nenenib::core
