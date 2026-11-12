#pragma once

#include <cstdint>

namespace nenenib::core
{
// Vim の 1 鍵が「undo の単位を作ったか」と「本文を変えたと数えるか」の印（ADR 0181 の決定 1）。
// changed は Vim が u_save と changed_*（b:changedtick・'modified'・`'.`）を両方呼ぶ形、saved_only
// は u_save だけを呼ぶ形（英字の無い大小・行頭の `X` `d0`・空の行の `x` `C`・空のレジスタの `p`
// ほか）。どちらも呼ばない鍵は VimStep.change を空にする。「changed_* だけ」は Vim に無いので値を
// 置かない。値が増えたら switch が落ちる（CPP-002）。本文が結果として同じかどうかでは決めない。
enum class VimChange : std::uint8_t
{
    changed,
    saved_only
};
} // namespace nenenib::core
