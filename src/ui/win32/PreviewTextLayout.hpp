#pragma once

#include "PreviewBlockKind.hpp"
#include "PreviewInlineBox.hpp"
#include "PreviewSpan.hpp"
#include "PreviewTextStyle.hpp"

#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <optional>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// PreviewTextLayouts が保持する 1 要素の文字組み（ADR 0151 の決定 16）。鍵は要素の文字・装飾・
// 種類・段・幅（物理画素）・字体の設定で、全部が一致するときだけ使う。height は
// GetMetrics().height（物理画素）。inlines は読めた画像の箱（ADR 0155 の追補 62・無ければ空）。
// 描くときに layout の書式を変えず位置も問わないため、行内コードの面の矩形（code_runs・layout の
// 原点から）と画像の箱の左上（image_origins・inlines と同じ順）は作るときに 1 回だけ引いて持ち、
// 色（drawing effect）は最初に描くときに 1 回だけ置く。inked はその色を置いた brush の
// PreviewBrushes::generation（0 は未だ置いていない・#1477・ADR 0155 の追補）。
struct PreviewTextLayout
{
    std::string text;
    std::vector<core::PreviewSpan> spans;
    core::PreviewBlockKind kind;
    std::uint8_t level;
    float width;
    PreviewTextStyle style;
    std::vector<PreviewInlineBox> inlines;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    float height;
    std::vector<DWRITE_HIT_TEST_METRICS> code_runs;
    std::vector<std::optional<D2D1_POINT_2F>> image_origins;
    std::uint64_t inked = 0;
};
} // namespace nenenib::ui::win32
