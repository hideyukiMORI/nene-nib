#pragma once

#include <cstddef>

namespace nenenib::application
{
// ADR 0139（提案・試作）: 巨大な行の表示値が持つのは窓の中の本文だけ。窓の始まりの、元の行の
// 中のバイト位置・その前のコードポイント数・見積もった x（px・整数値）を運ぶ。表示文字列の中の
// 窓の始まりは常に 0。active でない行は今日どおり行の全体。
struct LineWindow
{
    bool active = false;
    std::size_t byte = 0;
    std::size_t column = 0;
    float x = 0;
};
} // namespace nenenib::application
