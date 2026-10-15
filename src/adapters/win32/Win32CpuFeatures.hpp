#pragma once

#include "simd/CpuFeatures.hpp"

namespace nenenib::adapters::win32
{
// CPU の機能を OS に尋ねる（ADR 0006 の決定 2）。AVX2 の判定は OS が XSAVE の状態まで見る。
[[nodiscard]] core::simd::CpuFeatures read_cpu_features() noexcept;
} // namespace nenenib::adapters::win32
