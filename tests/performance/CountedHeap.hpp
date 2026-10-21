#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// probe の exe の operator new / delete を置き換え、数えている間に増えた確保の byte 数を返す
// （#792・改行の索引の payload を製品に測定専用の API を足さずに数える）。数えるのは start から
// stop の間だけで、その外の確保と解放は数えない。窓なしの probe は 1 本のスレッドで流れる。
void start_heap_count() noexcept;
[[nodiscard]] std::int64_t counted_heap_bytes() noexcept;
void stop_heap_count() noexcept;
} // namespace nenenib::tests::performance
