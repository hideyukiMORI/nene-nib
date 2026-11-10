#pragma once

#include "EditorSettings.hpp"

#include <windows.h>

#include <cstdint>

#include <string>
#include <string_view>
#include <tuple>
#include <utility>

// EditorWindow の群（ADR 0158 の追補 7）の 2 つ以上が使う補助と定数。窓の .cpp だけが include
// する。
namespace nenenib::ui::win32::detail
{
inline constexpr wchar_t product_name[] = L"NeNe Nib";
// 本文の行数と字幅（巨大な行の窓の見積もり・ADR 0139）を決める字の大きさと字体と、本文の幅を
// 縮めるミニマップを出すか（ADR 0171 の決定 1・2）。変われば窓は寸法を送り直す。ミニマップは
// この既にある比べの 1 欄で、打鍵の道に比べを足さない。
using FontKey = std::tuple<float, std::string, bool>;
[[nodiscard]] FontKey font_of(const core::EditorSettings &settings, bool minimap);
[[nodiscard]] bool held(int key) noexcept;
[[nodiscard]] std::wstring widen(std::string_view utf8);
// マウスのメッセージの LPARAM の符号つきの x と y（窓の外へ出たドラッグでは負になる）。
[[nodiscard]] std::int32_t low_word_of(LPARAM data) noexcept;
[[nodiscard]] std::int32_t high_word_of(LPARAM data) noexcept;
} // namespace nenenib::ui::win32::detail
