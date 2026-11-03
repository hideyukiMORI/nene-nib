#pragma once

#include "LspPosition.hpp"

namespace nenenib::core
{
// LSP の Range（start と end の Position・end は含まない・ADR 0161 の決定 2）。公開 aggregate。
struct LspRange
{
    LspPosition start;
    LspPosition end;
};

[[nodiscard]] constexpr bool operator==(const LspRange &left, const LspRange &right) noexcept
{
    return left.start == right.start && left.end == right.end;
}
} // namespace nenenib::core
