#pragma once

#include "ContextMenuMode.hpp"

namespace nenenib::core
{
// メニューを開いた時点の本文の様子（ADR 0078 の追補 #1051）。項目が使えるかと鍵の表示はこの 2 つ
// だけで決まる。クリップボードは開くたびに読まない（空の貼り付けは試して失敗を知らせる）。
struct ContextMenuSituation
{
    ContextMenuMode mode;
    // 本文に選んだ範囲がある（Vim の VISUAL の選択は mode の側で表す）。
    bool selected;
};
} // namespace nenenib::core
