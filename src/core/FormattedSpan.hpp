#pragma once

#include "Offset.hpp"
#include "OffsetRange.hpp"

#include <string>

namespace nenenib::core
{
// 整形の答えの全部の `TextEdit` を覆う 1 区間の置き換え（ADR 0191 の決定 7）。range は今の本文の
// 上の区間（最小の始まり〜最大の終わり）、text はその区間の新しい本文、caret は置き換えた後の
// 本文の上のキャレット（当てる前のキャレットを編集の前後の差で写した位置）。公開 aggregate
// （CPP-003）。
struct FormattedSpan
{
    OffsetRange range;
    std::string text;
    Offset caret;
};
} // namespace nenenib::core
