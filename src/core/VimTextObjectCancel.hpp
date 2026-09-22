#pragma once

#include "Selection.hpp"

namespace nenenib::core
{
// テキストオブジェクトが範囲にならなかったとき（回数が本文で尽きた・対が見つからない）。
// 保留中のオペレータは捨て、選択（NORMAL ではキャレット）がこの値になる。語は走査が止まった
// 所へキャレットが動き（前向きなら本文の終わり・後ろ向きなら本文の先頭）、引用符と括弧は
// 元のまま動かない（どちらも Vim 9.1 で実測・ADR 0031 の補足）。本文は変わらないので、
// `.` の記録は捨てられる（ADR 0030 の決定 3）。
struct VimTextObjectCancel
{
    Selection selection;
};
} // namespace nenenib::core
