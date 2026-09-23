#pragma once

#include <cstdint>

namespace nenenib::core
{
// 次の鍵で完成するNORMAL/VISUAL命令の閉じた接頭辞（ADR 0027 / 0029）。q と @ は次の鍵が
// マクロのレジスタの名前（ADR 0046 の決定 2・3）。
enum class VimPrefix : std::uint8_t
{
    g,
    r,
    q,
    at
};
} // namespace nenenib::core
