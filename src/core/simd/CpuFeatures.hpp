#pragma once

namespace nenenib::core::simd
{
// 合成ルートが OS から読んだ CPU の機能（ADR 0006 の決定 2）。core は cpuid を読まない。
// x64 の SSE2 は常にあるので持たない。
struct CpuFeatures
{
    bool avx2 = false;
};
} // namespace nenenib::core::simd
