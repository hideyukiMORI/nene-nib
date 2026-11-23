#pragma once

#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "VimBlockEdit.hpp"

#include <string>
#include <vector>

namespace nenenib::core
{
// 整形の答えの全部の `TextEdit` を覆う 1 区間の置き換え（ADR 0191 の決定 7）。range は今の本文の
// 上の区間（最小の始まり〜最大の終わり）、text はその区間の新しい本文、caret は置き換えた後の
// 本文の上のキャレット（当てる前のキャレットを編集の前後の差で写した位置）。pieces は区間を組んだ
// 編集の列（今の本文の上の区間と新しい本文・始まりの順）で、マークともう 1 つの枠を編集ごとに直す
// ために replace の第 4 引数（FormattingPieces）へ渡す（#1997 のレビュー R1）。公開 aggregate
// （CPP-003）。
struct FormattedSpan
{
    OffsetRange range;
    std::string text;
    Offset caret;
    std::vector<VimBlockEdit> pieces;
};
} // namespace nenenib::core
