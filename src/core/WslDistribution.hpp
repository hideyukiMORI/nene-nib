#pragma once

#include <cstdint>
#include <string>

namespace nenenib::core
{
// 登録された WSL のディストリ 1 つ（ADR 0162 の追補 #1450）。名前は UTF-8、state はレジストリの
// `State` の値のまま（1 が入れ終わった）。どれを場所にするかは wsl_places が決める。
struct WslDistribution
{
    std::string name;
    std::uint32_t state = 0;
};
} // namespace nenenib::core
