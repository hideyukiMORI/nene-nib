#pragma once

#include "EditMode.hpp"

namespace nenenib::core
{
// メニューを開いた時点の本文の様子（ADR 0078 の追補 #1051）。項目が使えるかと鍵の表示はこの 3 つ
// だけで決まる。クリップボードは開くたびに読まない（空の貼り付けは試して失敗を知らせる）。
struct ContextMenuSituation
{
    EditMode mode;
    // 本文に選んだ範囲がある（Vim の VISUAL を含む）。
    bool selected;
    // 本文を OS のクリップボードの鍵で書き換えてよい（通常モードと Vim の INSERT・全面プレビュー
    // でない）。
    bool editable;
};
} // namespace nenenib::core
