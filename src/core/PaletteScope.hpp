#pragma once

#include <cstdint>

namespace nenenib::core
{
// Ctrl+P の面が見せる出どころ（ADR 0060 の決定 2）。files は印のある候補の全部、tabs は開いて
// いるタブ、history は閉じたファイルの履歴、commands は Ex の設定のコマンド。値が増えたら写す
// switch が落ちる（CPP-002）。
enum class PaletteScope : std::uint8_t
{
    files,
    tabs,
    history,
    commands
};
} // namespace nenenib::core
