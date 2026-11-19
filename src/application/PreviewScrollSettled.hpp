#pragma once

#include "Offset.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::application
{
// ui がプレビューの縦の送りを畳んだ結果（ADR 0155 の決定 7 の 2026-11-20 の追補・#1894）。ui は描く
// 前に (錨の要素, nudge + pending × 行の送り) を要素の高さで正規化し、描いた後に変わったときだけ
// 1 回送る。anchor は枠の上端に来る要素の source_begin、lines は囲みのコードの要素で枠の上端に
// 掛かる行の要素の中の番号（ほかの要素は 0・application が本文の行頭へ写す）、nudge はその要素の
// 上端から枠の上端までの DIP（0 以上で要素の高さ未満）、consumed は畳んだ送りの量（frame の
// pending の写し・application は引き算するので、間に来た送りを失わない）。使う人の操作ではない
// （is_signal）。どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct PreviewScrollSettled
{
    core::Offset anchor;
    std::size_t lines;
    float nudge;
    std::int64_t consumed;
};
} // namespace nenenib::application
