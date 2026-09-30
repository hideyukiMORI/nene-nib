#pragma once

#include "PaletteScope.hpp"

#include <string_view>

namespace nenenib::core
{
// 入力を出どころと検索の文字に分けたもの（ADR 0060 の決定 2）。query は入力の借用。
struct PaletteQuery
{
    PaletteScope scope;
    std::string_view query;
};
} // namespace nenenib::core
