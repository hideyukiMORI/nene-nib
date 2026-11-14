#pragma once

#include <cstdint>

namespace nenenib::core
{
// 起動画面の名乗りをどこまで描くか（施主決定 D90・#1677）。none は題だけ、version
// は題の右の版まで、 version_and_signature は版と一覧の下の署名 2 行まで。狭い窓で落とす順は署名 →
// 版で、版を落とした ら署名も落とす（start_credit の 1 本が決める）。
enum class StartCredit : std::uint8_t
{
    none,
    version,
    version_and_signature
};
} // namespace nenenib::core
