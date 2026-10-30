#pragma once

#include "LayoutRect.hpp"
#include "MarkdownDocument.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewSelection.hpp"
#include "PreviewTextStyle.hpp"

#include <optional>

namespace nenenib::ui::win32
{
// PreviewPainter に 1 回の描画で渡すもの（ADR 0151 の決定 17）。文書と縦の位置は借用で、描く間だけ
// 生かす。frame は枠の矩形、scroll は上の送り（どちらも描画先の物理画素）。selection
// は全面プレビューの 選択（ADR 0155 の改訂 18・選択の面を文字の下に塗る・無ければ nullopt）。
struct PreviewScene
{
    const core::MarkdownDocument &document;
    const PreviewBlockTops &tops;
    const PreviewTextStyle &style;
    core::LayoutRect frame;
    float scroll;
    std::optional<core::PreviewSelection> selection;
};
} // namespace nenenib::ui::win32
