#pragma once

#include "BodyGlyphRun.hpp"
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
    // 巨大な行の窓の layout は書式の折り返しにかかわらず折り返さない（ADR 0139・試作）。
    bool unwrapped = false;
    float glyph_left = 0;
};
} // namespace nenenib::ui::win32
