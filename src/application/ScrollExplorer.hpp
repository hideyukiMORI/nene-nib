#pragma once

#include <cstdint>

namespace nenenib::application
{
// 面のホイール（ADR 0162 の決定 4）。見えている行の先頭 first_visible を lines 行送る。正が下。
// 行数で切り詰め、選択は動かさない。面が消えていれば何もしない。
struct ScrollExplorer
{
    std::int32_t lines;
};
} // namespace nenenib::application
