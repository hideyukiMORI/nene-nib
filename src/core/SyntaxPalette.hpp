#pragma once

#include "RgbColor.hpp"

namespace nenenib::core
{
// テーマ 1 つ分の本文トークン 16 個（ADR 0017 の決定 1）。base16 の 16 色を Nib の役割名で持つ。
// どのメンバーも単独で妥当な色なので公開 aggregate（ADR 0007 / CPP-003）。
// UI トークン（Palette）とは変わる理由が違うので別の値型で、Theme が両方を持つ。
// ハイライトの縦切りがこれを本文に塗る（Issue #52 では塗らない）。
struct SyntaxPalette
{
    RgbColor foreground;
    RgbColor background;
    RgbColor cursor;
    RgbColor selection;
    RgbColor current_line;
    RgbColor line_number;
    RgbColor comment;
    RgbColor keyword;
    RgbColor string;
    RgbColor number;
    RgbColor type;
    RgbColor function;
    RgbColor constant;
    // ADR 0017 の役割名は operator だが、C++ の予約語なので複数形で持つ。
    RgbColor operators;
    RgbColor error;
    RgbColor warning;
};
} // namespace nenenib::core
