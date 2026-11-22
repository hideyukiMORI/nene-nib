#pragma once

#include <cstdint>

namespace nenenib::core
{
// 設定の面の行の領域に並ぶもの（#1836）。heading は節の見出し、summary は機能の節の要約と組、
// summary_note は要約の箱の下の小さな 1 行（段の決め方）、row は行（設定の行と「言語サーバー」の
// 節の行・#1945）、empty は当たる行が無いときの 1 行。
enum class SettingsItemKind : std::uint8_t
{
    heading,
    summary,
    summary_note,
    row,
    empty
};
} // namespace nenenib::core
