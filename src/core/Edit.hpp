#pragma once

#include "Offset.hpp"

#include <string>

namespace nenenib::core
{
// 1 つの編集。at から removed が消え、代わりに inserted が入った、という記録（ADR 0009 の決定 3）。
// undo は inserted を removed に戻し、redo はその逆を行う。4 つは互いに独立して妥当な値。
// restore はその編集を始める瞬間のキャレット（編集の前の本文の上の位置）で、Vim の u と Ctrl-r が
// 戻る先（ADR 0052 の決定 1）。既定値は付けない＝構築する所はすべて書く。
struct Edit
{
    Offset at;
    std::string removed;
    std::string inserted;
    Offset restore;
};

[[nodiscard]] inline bool operator==(const Edit &left, const Edit &right)
{
    return left.at == right.at && left.removed == right.removed &&
           left.inserted == right.inserted && left.restore == right.restore;
}
} // namespace nenenib::core
