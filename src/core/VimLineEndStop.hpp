#pragma once

#include <cstdint>

namespace nenenib::core
{
// `<Space>` `<BS>` の 1 歩が行末の位置（改行のオフセット・Vim が NUL を置く桁）に止まるか
// （ADR 0049 の決定 2）。裸の NORMAL は止まらずに隣の行へ渡り、オペレータ待ちと VISUAL は一度
// 止まる。止まるのは Vim の nv_right / nv_left が行末で 1 歩ぶん足踏みするのと同じ。
enum class VimLineEndStop : std::uint8_t
{
    passes,
    rests
};
} // namespace nenenib::core
