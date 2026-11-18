#pragma once

#include <cstdint>

namespace nenenib::core
{
// 設定の面の行の領域に並ぶもの（#1836）。heading は節の見出し、summary は機能の節の要約と組、
// row は設定の行、command は言語サーバーの命令の行（読むだけ）、empty は当たる行が無いときの 1 行。
enum class SettingsItemKind : std::uint8_t
{
    heading,
    summary,
    row,
    command,
    empty
};
} // namespace nenenib::core
