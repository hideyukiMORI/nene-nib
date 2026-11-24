#pragma once

#include "OffsetRange.hpp"

#include <string>

namespace nenenib::core
{
// 本文の `range` を `utf8` で置き換える 1 つの編集。矩形の編集の 1 行ぶん（ADR 0035 の決定 3）では
// 本文も caret も LF 換算で、文書の改行へ直すのは controller の 1 か所（ARC-009）。整形の編集の列
// （FormattedSpan.pieces・FormattingPieces・FormattedUnit・ADR 0191 の決定 7）は、range が今の
// 本文の上の区間で utf8 が文書の改行の形（CRLF の文書なら CRLF）のまま持つ（marks_block_adjusted は
// 改行のバイトを数えるだけなので、どちらの形でも同じに直す）。
struct VimBlockEdit
{
    OffsetRange range;
    std::string utf8;
};
} // namespace nenenib::core
