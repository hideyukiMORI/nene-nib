#pragma once

#include "CpuFeatures.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace nenenib::core::simd
{
// ASCII の連続区間と LF の位置を数える走査の実装（CPP-018・ADR 0006）。scalar が fallback で、
// どれを使うかは合成ルートが select_ascii_newline_scan で 1 回決めて注入する。
enum class AsciiNewlineScan : std::uint8_t
{
    scalar,
    sse2,
    avx2
};

[[nodiscard]] AsciiNewlineScan select_ascii_newline_scan(const CpuFeatures &features) noexcept;

// index から先で最初の非 ASCII の位置（無ければ text.size()）。
[[nodiscard]] std::size_t ascii_end(AsciiNewlineScan scan, std::string_view text,
                                    std::size_t index) noexcept;

// text の各 LF について words の (bit + 位置) の bit を立て、LF の数を返す。
// words は bit + text.size() までの bit を持っていなければならない。
[[nodiscard]] std::size_t mark_newlines(AsciiNewlineScan scan, std::string_view text,
                                        std::span<std::uint64_t> words, std::size_t bit) noexcept;
} // namespace nenenib::core::simd
