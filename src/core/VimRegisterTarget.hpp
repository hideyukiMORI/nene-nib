#pragma once

#include <cstdint>

namespace nenenib::core
{
// `"` の接頭辞が選ぶ書き先と読み元の種類（ADR 0048 の決定 2）。named は a〜z の表の 1 本
// （追記かどうかは VimRegisterSelection.append）、unnamed は `""`、black_hole は `"_`。
// numbered は `"0`〜`"9` の表の 1 本、small_delete は `"-`（ADR 0050 の決定 2）。
// clipboard は `"+` と `"*`（同じ 1 つの書き先・読み元で、違いは打った鍵だけ・ADR 0051 の決定 1）。
// 写し先が足りなければ switch がコンパイルで落ちる（CPP-002）。
enum class VimRegisterTarget : std::uint8_t
{
    named,
    unnamed,
    black_hole,
    numbered,
    small_delete,
    clipboard
};
} // namespace nenenib::core
