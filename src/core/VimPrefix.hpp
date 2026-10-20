#pragma once

#include <cstdint>

namespace nenenib::core
{
// 次の鍵で完成するNORMAL/VISUAL命令の閉じた接頭辞（ADR 0027 / 0029）。q と @ は次の鍵が
// マクロのレジスタの名前（ADR 0046 の決定 2・3）。quote は `"` で、次の鍵が書き先と読み元の
// レジスタの名前（ADR 0048 の決定 2）。z は横送りの鍵（ADR 0145 の決定 5）。window_command は
// `Ctrl-w` で、次の鍵が窓の命令（ADR 0148 の決定 7）。
enum class VimPrefix : std::uint8_t
{
    g,
    r,
    q,
    at,
    quote,
    z,
    window_command,
    // `m` `'` `` ` ``。次の鍵がマークの名前（ADR 0152 の決定 3）。
    mark,
    mark_line,
    mark_exact
};
} // namespace nenenib::core
