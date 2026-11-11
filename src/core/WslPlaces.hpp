#pragma once

#include "WslDistribution.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace nenenib::core
{
// レジストリの `State` の「入れ終わった」の値（入れている途中・外している途中は場所にしない）。
inline constexpr std::uint32_t wsl_installed_state = 1;
// 箱に足す場所の数の上限。壊れた・残った登録が多くても、箱を出す前の解決を UI の糸で重ねない。
inline constexpr std::size_t wsl_place_limit = 16;
// ディストリの名前の長さの上限（UTF-8 のバイト）。Windows の経路の 1 節は 255 文字までで、
// レジストリの副鍵の名前の上限とも同じ。これより長い名前は経路の 1 節にならないので外す。
inline constexpr std::size_t wsl_name_limit = 255;

// フォルダ選びの箱の左の一覧に足す WSL の場所（ADR 0162 の追補 #1450）。入れ終わったディストリ
// ごとに `\\wsl.localhost\<名前>` を登録の順で、最大 wsl_place_limit 件返す（超えた分は捨てる）。
// 名前が空・長すぎる・`.` `..`・UNC の区切りや Windows の経路に使えない文字を含むもの、Docker が
// 内部で使う名前、同じ名前の 2 つ目（どちらも ASCII の大小を無視して比べる）は外す。WSL
// が無ければ空。
[[nodiscard]] std::vector<std::string> wsl_places(std::span<const WslDistribution> distributions);
} // namespace nenenib::core
