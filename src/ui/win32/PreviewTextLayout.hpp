#pragma once

#include "PreviewBlockKind.hpp"
#include "PreviewInlineBox.hpp"
#include "PreviewSpan.hpp"
#include "PreviewTextStyle.hpp"

#include <cstdint>
#include <dwrite.h>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// PreviewTextLayouts が保持する 1 要素の文字組み（ADR 0151 の決定 16）。鍵は要素の文字・装飾・
// 種類・段・幅（物理画素）・字体の設定で、全部が一致するときだけ使う。height は
// GetMetrics().height（物理画素）。inlines は読めた画像の箱（ADR 0155 の追補 62・無ければ空）。
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
};
} // namespace nenenib::ui::win32
