#pragma once

#include <cstdint>

namespace nenenib::core
{
// Ex の窓（枠）の命令（ADR 0148 の決定 5）。`:only` はもう 1 つの枠を閉じる。2 枠のときの
// `:q` 系が枠を閉じるかは controller が枠の数で決める（core は枠の数を知らない）。
enum class ExWindowCommand : std::uint8_t
{
    only
};
} // namespace nenenib::core
