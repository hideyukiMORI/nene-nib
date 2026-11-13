#pragma once

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// 解決済みの表示言語（ADR 0123 の追補 2026-11-13 の決定 1・#1019）。文の表の列が 1 つずつある
// 言語だけを持つ。希望値 DisplayLanguageChoice（設定の 7 値）は resolved_language がここへ写す。
// 列を足すときは 1 値足し、落ちた switch（UiTexts.cpp の列の選択・resolved_language）を埋める。
enum class DisplayLanguage : std::uint8_t
{
    japanese,
    english,
    simplified_chinese
};

// 値の数（文の表の列の数）。
inline constexpr std::size_t display_language_count = 3;
} // namespace nenenib::core
