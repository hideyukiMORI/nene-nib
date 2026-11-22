#pragma once

#include <cstdint>

namespace nenenib::core
{
// 候補を選んだときの動き。operate は操作の一覧の候補で、面を閉じて頼まれた操作を立てる
// （ADR 0078 の決定 6・8）。land は診断の候補で、command は controller が開くときに控えた診断の
// 列の位置（10 進）。選ぶとその文書の位置へ跳ぶ（#1939）。
enum class CommandChoiceKind : std::uint8_t
{
    fill,
    execute,
    open,
    operate,
    land
};
} // namespace nenenib::core
