#pragma once

#include "BodyGlyphRun.hpp"
#include "DisplayForm.hpp"
#include "LayoutRect.hpp"

#include <dwrite.h>
#include <string>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// 共通本文キャッシュが所有する文字組み。本文の正本ではなく、全表示文字列と幾何が一致する
// ときだけ使うCOM資源。書式変更で全て破棄する（ADR 0069）。
struct BodyTextLayout
{
    std::string text;
    core::LayoutRect area;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    std::vector<BodyGlyphRun> glyphs{};
    bool glyphs_ready = false;
    bool glyphs_tried = false;
    std::size_t rows = 1;
    float glyph_left = 0;
    // 格子の画面行は等幅の書式で升に揃えた layout（ADR 0140 の決定 5）。同じ文字列でも text
    // とは別。
    core::DisplayForm form = core::DisplayForm::text;
    // 巨大な行の窓の layout は折り返さない（ADR 0139 の決定 4）。同じ文字列でも折り返す layout
    // とは別。
    bool unwrapped = false;
};
} // namespace nenenib::ui::win32
