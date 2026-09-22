#pragma once

#include "VimSearchDirection.hpp"

#include <string>

namespace nenenib::core
{
// 確定した検索（ADR 0032 の決定 3）。入力行の Enter がこの鍵 1 つになって engine に届き、
// `.` の記録にも鍵として残る（追加の記録の仕組みは要らない）。空のパターンは直前の使い直し。
// 値は単独で妥当なので公開 aggregate で、比較は非メンバー（CPP-003）。
struct VimSearchPattern
{
    std::string pattern;
    VimSearchDirection direction;
};

[[nodiscard]] inline bool operator==(const VimSearchPattern &left, const VimSearchPattern &right)
{
    return left.direction == right.direction && left.pattern == right.pattern;
}
} // namespace nenenib::core
