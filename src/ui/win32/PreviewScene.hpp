#pragma once

#include "LayoutRect.hpp"
#include "MarkdownDocument.hpp"
#include "PreviewBitmaps.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewImages.hpp"
#include "PreviewSelection.hpp"
#include "PreviewTextStyle.hpp"

#include <optional>

namespace nenenib::ui::win32
{
// PreviewPainter に 1 回の描画で渡すもの（ADR 0151 の決定 17）。文書と縦の位置は借用で、描く間だけ
// 生かす。frame は枠の矩形、scroll は上の送り（どちらも描画先の物理画素）。selection
// は全面プレビューの 選択（ADR 0155 の改訂 18・選択の面を文字の下に塗る・無ければ nullopt）。
// images は画像の状態、bitmaps は読めた画像の bitmap（ADR 0155 の追補 62・63・どちらも null なら
// 画像は「[画像: alt]」の文字のまま描く）。
struct PreviewScene
{
    const core::MarkdownDocument &document;
    const PreviewBlockTops &tops;
    const PreviewTextStyle &style;
    core::LayoutRect frame;
    float scroll;
    std::optional<core::PreviewSelection> selection;
    const application::PreviewImages *images = nullptr;
    const PreviewBitmaps *bitmaps = nullptr;
};
} // namespace nenenib::ui::win32
