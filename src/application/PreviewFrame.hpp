#pragma once

#include "DisplayText.hpp"
#include "MarkdownDocument.hpp"
#include "Offset.hpp"
#include "PreviewImages.hpp"
#include "PreviewSelection.hpp"

#include <memory>
#include <optional>

namespace nenenib::application
{
// プレビューの枠の表示値（ADR 0155 の決定 11）。PreviewView の値の写しだけで、本文を歩かない。
// document は見せる結果（まだ無ければ空）、anchor は枠の上端に来るブロックを引く本文のバイト位置
// （anchor を含むブロックの上端を枠の上端に置くのは ui・決定 7）、notice は 1 行の知らせ、selection
// は全面プレビューの選択（改訂 18・側の枠は持たない）。images は画像の状態（ADR 0155 の追補 52・
// null なら画像の節が無い）。序数の画像は images->pixels_of が値を返すときだけ画素で描き、ほかは
// 「[画像: alt]」の文字のまま。images の指す先が変われば ui は錨から測り直す（段 3）。
struct PreviewFrame
{
    std::shared_ptr<const core::MarkdownDocument> document;
    core::Offset anchor;
    std::optional<core::DisplayText> notice;
    std::optional<core::PreviewSelection> selection;
    std::shared_ptr<const PreviewImages> images = nullptr;
};
} // namespace nenenib::application
