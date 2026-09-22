#pragma once

#include <cstdint>

namespace nenenib::core
{
// Vim のモードの閉じた一覧（ADR 0012 の決定 1 / ADR 0018 の決定 1 / ADR 0035 の決定 1）。
// VISUAL は種類ごとに 1 値で、選択の形が状態から曖昧にならない（コマンドラインは次の縦切り）。
// 値が増えたら写し先の足りない `switch` がコンパイルで落ちる（CPP-002）。
enum class VimMode : std::uint8_t
{
    normal,
    insert,
    visual,
    visual_line,
    visual_block
};
} // namespace nenenib::core
