#pragma once

#include "PreviewBlockKind.hpp"
#include "PreviewCodeColor.hpp"
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
// ranges は spans と同じ順の layout の UTF-16 の範囲で、作るときに 1 回だけ写して
// 装飾・面の矩形・色が読む（#1485）。colors は囲みのコードの構文の色の区間（要素の
// code_colors・鍵に入る）で、color_ranges は同じ順の UTF-16 の範囲（#1858）。
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
    std::vector<DWRITE_TEXT_RANGE> ranges;
    // 最後に引いた回（PreviewTextLayouts の通し番号・あふれたら最も小さい entry を捨てる）。
    std::uint64_t used = 0;
    std::optional<std::vector<core::PreviewCodeColor>> colors;
    std::vector<DWRITE_TEXT_RANGE> color_ranges;
};
} // namespace nenenib::ui::win32
