#pragma once

#include "Offset.hpp"

#include <string>

namespace nenenib::core
{
// 1 つの編集。at から removed が消え、代わりに inserted が入った、という記録（ADR 0009 の決定 3）。
// undo は inserted を removed に戻し、redo はその逆を行う。3 つは互いに独立して妥当な値。
struct Edit
{
    Offset at;
    std::string removed;
    std::string inserted;
};

[[nodiscard]] inline bool operator==(const Edit &left, const Edit &right)
{
    return left.at == right.at && left.removed == right.removed && left.inserted == right.inserted;
}
} // namespace nenenib::core
