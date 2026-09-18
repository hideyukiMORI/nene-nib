#pragma once

#include "Appearance.hpp"
#include "Palette.hpp"
#include "SyntaxPalette.hpp"
#include "ThemeSource.hpp"

#include <string_view>

namespace nenenib::core
{
// テーマ 1 つ分の値（ADR 0017 の決定 3）。名前は Vim 流の小文字ハイフン（solarized-dark）。
// 組み込みは BuiltinThemes.hpp の constexpr の表が持ち、C4 の利用者のテーマファイルも
// adapters が読んで同じ値型を作る（ADR 0008 の決定 8）。
struct Theme
{
    std::string_view name;
    Appearance appearance;
    Palette ui;
    SyntaxPalette body;
    ThemeSource source;
};
} // namespace nenenib::core
