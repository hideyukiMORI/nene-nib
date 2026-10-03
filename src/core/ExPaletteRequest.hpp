#pragma once

#include "PaletteScope.hpp"

#include <string>

namespace nenenib::core
{
// Ex が共通の一覧へ渡す出どころと検索文字（ADR 0064）。ファイルを開くのは候補の確定時だけ。
struct ExPaletteRequest
{
    PaletteScope scope;
    std::string query;
};
} // namespace nenenib::core
