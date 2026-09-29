#pragma once

#include "TabJumpDirection.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// どのタブへ行くかの頼み（ADR 0057 の決定 1）。count は `{N}gt` `{N}gT` と `:tabnext N`
// `:tabprevious N` の N で、無いときは隣へ行く。
struct TabJump
{
    TabJumpDirection direction;
    std::optional<std::size_t> count;
};
} // namespace nenenib::core
