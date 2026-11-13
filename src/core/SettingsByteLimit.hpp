#pragma once

#include <cstddef>

namespace nenenib::core
{
// 設定ファイルの大きさの上限（バイト・#1044 の 16 KiB）。正本はここ 1 つで、adapters の読み書きの
// 上限と ui の箱の文（「16 KiB（16384 バイト）」の 2 つの置き場）が同じ値を読む（#1019・V-1598 の
// C-L1）。
inline constexpr std::size_t maximum_settings_bytes = 16384;
} // namespace nenenib::core
