#pragma once

#include <cstdint>
#include <utility>

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

// VISUAL のどれかか（段落と文のテキストオブジェクトが広げ方を決めるのに読む・Issue #729）。
[[nodiscard]] constexpr bool is_visual_mode(VimMode mode) noexcept
{
    switch (mode)
    {
    case VimMode::normal:
    case VimMode::insert:
        return false;
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        return true;
    }
    std::unreachable();
}
} // namespace nenenib::core
