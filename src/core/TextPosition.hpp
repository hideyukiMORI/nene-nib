#pragma once

#include "Column.hpp"
#include "LineNumber.hpp"

namespace nenenib::core
{
// 人が読む位置（行と桁）。バイト位置 Offset との変換は TextBuffer の境界でだけ行う（ADR 0009）。
// 行と桁は互いに独立して妥当なので公開 aggregate（ADR 0007）。
struct TextPosition
{
    LineNumber line;
    Column column;
};

[[nodiscard]] constexpr bool operator==(const TextPosition &left,
                                        const TextPosition &right) noexcept
{
    return left.line == right.line && left.column == right.column;
}
} // namespace nenenib::core
