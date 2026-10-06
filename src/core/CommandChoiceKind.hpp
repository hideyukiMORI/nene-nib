#pragma once

#include <cstdint>

namespace nenenib::core
{
// 候補を選んだときの動き。operate は操作の一覧の候補で、面を閉じて頼まれた操作を立てる
// （ADR 0078 の決定 6・8）。
enum class CommandChoiceKind : std::uint8_t
{
    fill,
    execute,
    open,
    operate
};
} // namespace nenenib::core
