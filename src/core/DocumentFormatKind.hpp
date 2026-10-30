#pragma once

#include <cstdint>

namespace nenenib::core
{
// 保存の形のどちらの欄か（ADR 0010 の追補 #1049 の決定 2）。面を開く入口と `?` の問いが運ぶ。
enum class DocumentFormatKind : std::uint8_t
{
    encoding,
    line_ending
};
} // namespace nenenib::core
