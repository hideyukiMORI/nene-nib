#pragma once

#include "DisplayText.hpp"
#include "MarkdownDocument.hpp"
#include "Offset.hpp"
#include "PreviewImages.hpp"
#include "PreviewSelection.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace nenenib::application
{
// プレビューの枠の表示値（ADR 0155 の決定 11）。PreviewView の値の写しだけで、本文を歩かない。
// document は見せる結果（まだ無ければ空）、anchor は枠の上端に来るブロックを引く本文のバイト位置
// （anchor を含むブロックの上端を枠の上端に置くのは ui・決定 7）、notice は 1 行の知らせ、selection
// は全面プレビューの選択（改訂 18・側の枠は持たない）。images は画像の状態（ADR 0155 の追補 52・
// null なら画像の節が無い）。序数の画像は images->pixels_of が値を返すときだけ画素で描き、ほかは
// 「[画像: alt]」の文字のまま。images の指す先が変われば ui は錨から測り直す（段 3）。
// table_scrolls は表の通し番号ごとの横の送りの量（DIP・無い表は 0・端へ寄せるのは ui
// が描く枠の寸法で core::preview_table_scroll_clamped・#1883）、table_lit
// はつまみを明るく描く表（つかんでいる表か、 マウスを載せた棒の表）。nudge と pending は
// PreviewView の写しで、ui は錨の要素の上端を -(nudge + pending × 行の送り) に置いて正規化し、
// 変わったら PreviewScrollSettled を送る（ADR 0155 の決定 7 の 2026-11-20 の追補・#1894）。
// anchor_block は PreviewView の写しで、錨の要素は preview_anchored_block の 1 本で引く。
struct PreviewFrame
{
    std::shared_ptr<const core::MarkdownDocument> document;
    core::Offset anchor;
    std::optional<core::DisplayText> notice;
    std::optional<core::PreviewSelection> selection;
    std::shared_ptr<const PreviewImages> images = nullptr;
    std::vector<std::int32_t> table_scrolls{};
    std::optional<std::size_t> table_lit{};
    float nudge = 0.0F;
    std::int64_t pending = 0;
    std::optional<std::size_t> anchor_block{};
};
} // namespace nenenib::application
