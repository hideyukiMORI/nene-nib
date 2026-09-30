#pragma once

#include <cstdint>
#include <string_view>

namespace nenenib::core
{
// Ctrl+P の候補の出どころの印（ADR 0060 の決定 3）。Ex のコマンドの候補は印なし。値が増えたら
// 出どころで絞る関数と補足の文言の switch が落ちる（CPP-002）。
enum class PaletteOrigin : std::uint8_t
{
    tab
};

// 行の右に添える補足の文言（ADR 0060 の決定 3）。
[[nodiscard]] std::string_view palette_origin_label(PaletteOrigin origin) noexcept;
} // namespace nenenib::core
