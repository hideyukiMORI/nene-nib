#pragma once

#include "DiagnosticSeverity.hpp"
#include "Palette.hpp"
#include "RgbColor.hpp"
#include "SyntaxPalette.hpp"

namespace nenenib::core
{
// 診断の下線の色（ADR 0161 の決定 4）。error は本文の error、warning は本文の warning、information
// と hint は UI の muted。色のトークンは足さない。重さを足すと switch が落ちる（CPP-002）。
// renderer と格子の描画はこの答えを brush に置くだけで、別の対応表を持たない。
[[nodiscard]] RgbColor diagnostic_color(const SyntaxPalette &body, const Palette &ui,
                                        DiagnosticSeverity severity) noexcept;
} // namespace nenenib::core
