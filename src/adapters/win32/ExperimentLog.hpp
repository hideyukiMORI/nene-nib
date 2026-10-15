#pragma once

#include <cstdint>
#include <string_view>

namespace nenenib::adapters::win32
{
// 実験（WARP の最初の frame）だけの口。製品の節目（Milestone）は増やさない。
// 環境変数が在り空でなければ真。
[[nodiscard]] bool experiment_flag(const wchar_t *name);
// 「プロセス生成からの ms」を前に付けた 1 行を OutputDebugStringA に出し、環境変数
// NIB_EXPERIMENT_LOG があればそのファイルへ追記する。
void experiment_note(std::string_view text);
// 実験（D1）: 点と QueryPerformanceCounter をメモリに積む（frame の中でファイルに触れない）。
void experiment_stamp(std::uint32_t point);
// 実験（D1）: 積んだ点を「stamp <点> <QPC> <周波数>」の行で NIB_EXPERIMENT_LOG へ書く。
void experiment_flush();
} // namespace nenenib::adapters::win32
