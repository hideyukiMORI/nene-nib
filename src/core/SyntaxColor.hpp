#pragma once

#include "RgbColor.hpp"
#include "SyntaxPalette.hpp"
#include "SyntaxRole.hpp"

namespace nenenib::core
{
// 構文の役割をテーマの本文トークンの色へ写す 1 本（#816・ADR 0156 の決定 10）。役割を足すと
// switch が落ちる（CPP-002）。renderer はこの答えを brush に置くだけで、別の対応表を持たない。
[[nodiscard]] RgbColor syntax_color(const SyntaxPalette &palette, SyntaxRole role) noexcept;
} // namespace nenenib::core
