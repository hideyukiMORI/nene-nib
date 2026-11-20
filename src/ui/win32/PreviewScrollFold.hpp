#pragma once

#include "PreviewScrollSpot.hpp"
#include "RenderFailure.hpp"

#include <cstddef>
#include <expected>
#include <functional>

namespace nenenib::ui::win32
{
// 要素 index を置き始める位置から次の要素の上端までの長さ（最後の要素はその下端まで・物理画素）。
// 描く所と同じ PreviewBlockTops の測り方で測る（PreviewPane::span_of）。
using PreviewSpanOf = std::function<std::expected<float, RenderFailure>(std::size_t)>;

// プレビューの縦の送りを畳む純関数（ADR 0155 の決定 7 の 2026-11-20 の追補・#1894）。from.nudge は
// 畳む前の量（錨の要素の nudge ＋ 送りの量 × 行の送り・負なら上へ）。下へは要素の長さを引きながら
// 次の要素へ、上へは前の要素の長さを足しながら戻る。端は、上は (要素 0, 0)、下は「最後の要素の
// 上端が枠の上端」と「文書の下端が枠の下端」の深い方。count は文書の要素の数（0 なら from
// のまま）、 height は枠の高さ（物理画素）。長さを測れなければ失敗を返す。
[[nodiscard]] std::expected<PreviewScrollSpot, RenderFailure>
preview_scroll_fold(PreviewScrollSpot from, std::size_t count, float height,
                    const PreviewSpanOf &span_of);
} // namespace nenenib::ui::win32
