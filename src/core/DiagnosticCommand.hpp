#pragma once

#include <cstdint>

namespace nenenib::core
{
// 診断を歩く・読む・並べる操作（#1939・ADR 0161 の決定 9 の追補）。next / previous は次・前の
// 診断へ跳ぶ（Vim の `]d` `[d`・通常モードの F8 / Shift+F8）、show はキャレットの上の診断の文の
// 全部を札で見せる（Vim の `gl`）、list は Ctrl+P の一覧を診断の出どころで開く。値が増えたら
// 写し先の switch が落ちる（CPP-002）。
enum class DiagnosticCommand : std::uint8_t
{
    next,
    previous,
    show,
    list
};
} // namespace nenenib::core
