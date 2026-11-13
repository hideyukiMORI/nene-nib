#pragma once

#include "VimPatternMarkKind.hpp"
#include "VimPatternQuantity.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 原子の列の間の構造の印 1 つ（ADR 0184 の決定 3・parse が作り、命令の列に組むときだけ読む）。at は
// 印が立つ原子の境（その時点の原子の数）、capture は番号のある群の番号（0 始まり・`\%(` と選択では
// 値なし）、quantity は閉じの印に付く群の量（量の無い群と、開きと選択では値なし）。値は単独で妥当
// なので公開 aggregate（CPP-003）。
struct VimPatternMark
{
    VimPatternMarkKind kind;
    std::size_t at;
    std::optional<std::size_t> capture;
    std::optional<VimPatternQuantity> quantity;
};
} // namespace nenenib::core
