#include "Win32CpuFeatures.hpp"

#include <windows.h>

namespace nenenib::adapters::win32
{
core::simd::CpuFeatures read_cpu_features() noexcept
{
    return core::simd::CpuFeatures{
        .avx2 = IsProcessorFeaturePresent(PF_AVX2_INSTRUCTIONS_AVAILABLE) != 0};
}
} // namespace nenenib::adapters::win32
