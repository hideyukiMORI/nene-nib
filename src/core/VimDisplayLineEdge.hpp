#pragma once

#include <cstdint>

namespace nenenib::core
{
// `g0` `gm` `g$` `g^` が着く画面行の所（ADR 0147 の決定 2・8）。横に送る文書では見えている升の
// 左端・中・右端、折り返す文書では今の画面行の始まり・始まり + 1 画面行の升の半分・終わり。
// first_non_blank は start と同じ所から最初の非空白へ進む。
enum class VimDisplayLineEdge : std::uint8_t
{
    start,
    middle,
    end,
    first_non_blank
};
} // namespace nenenib::core
