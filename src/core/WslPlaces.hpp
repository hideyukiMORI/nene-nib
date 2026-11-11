#pragma once

#include "WslDistribution.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace nenenib::core
{
// レジストリの `State` の「入れ終わった」の値（入れている途中・外している途中は場所にしない）。
inline constexpr std::uint32_t wsl_installed_state = 1;

// フォルダ選びの箱の左の一覧に足す WSL の場所（ADR 0162 の追補 #1450）。入れ終わったディストリ
// ごとに `\\wsl.localhost\<名前>` を登録の順で返す。名前が空・`.` `..`・UNC の区切りや Windows の
// 経路に使えない文字を含むものと、同じ名前の 2 つ目は外す。WSL が無ければ空。
[[nodiscard]] std::vector<std::string> wsl_places(std::span<const WslDistribution> distributions);
} // namespace nenenib::core
