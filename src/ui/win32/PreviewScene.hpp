#pragma once

#include "LayoutRect.hpp"
#include "MarkdownDocument.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewTextStyle.hpp"

namespace nenenib::ui::win32
{
// PreviewPainter に 1 回の描画で渡すもの（ADR 0151 の決定 17）。文書と縦の位置は借用で、描く間だけ
// 生かす。frame は枠の矩形、scroll は上の送り（どちらも描画先の物理画素）。
struct PreviewScene
{
    const core::MarkdownDocument &document;
    const PreviewBlockTops &tops;
    const PreviewTextStyle &style;
    core::LayoutRect frame;
    float scroll;
};
} // namespace nenenib::ui::win32
