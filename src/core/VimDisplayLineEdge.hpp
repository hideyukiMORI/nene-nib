#pragma once

#include <cstdint>

namespace nenenib::core
{
// `g0` `gm` `g$` が着く画面行の所（ADR 0147 の決定 2）。横に送る文書では見えている升の左端・
// 中・右端、折り返す文書では今の画面行の始まり・始まり + 1 画面行の升の半分・終わり。
enum class VimDisplayLineEdge : std::uint8_t
{
    start,
    middle,
    end
};
} // namespace nenenib::core
