#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::core
{
// ミニマップが bitmap に持つ行の窓（ADR 0171 の決定 4）。first から rows 行（文書の中に収まる）。
// どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct MinimapWindow
{
    LineNumber first;
    std::size_t rows;
};

[[nodiscard]] constexpr bool operator==(const MinimapWindow &left,
                                        const MinimapWindow &right) noexcept
{
    return left.first == right.first && left.rows == right.rows;
}
} // namespace nenenib::core
