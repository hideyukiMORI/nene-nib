#include "AsciiNewlineScan.hpp"

#include <immintrin.h>

#include <cstring>
#include <utility>

namespace nenenib::core::simd
{
namespace
{
constexpr std::size_t word_bits = 64;
constexpr std::size_t sse2_bytes = 16;
constexpr std::size_t avx2_bytes = 32;

// width bit の bits を words の bit 番目から OR する。word をまたぐときは次の word へ続ける。
void place_bits(std::span<std::uint64_t> words, std::size_t bit, std::uint64_t bits,
                std::size_t width) noexcept
{
    const std::size_t shift = bit % word_bits;
    words[bit / word_bits] |= bits << shift;
    if (shift + width > word_bits)
    {
        words[(bit / word_bits) + 1] |= bits >> (word_bits - shift);
    }
}

// 残りが word 全体に足りるときだけ memcpy で読み、アラインメント・別名参照・末尾の先読みを
// 要求しない（ADR 0086）。
[[nodiscard]] std::size_t ascii_end_scalar(std::string_view text, std::size_t index) noexcept
{
    constexpr std::uint64_t high_bits = 0x8080808080808080ULL;
    while (text.size() - index >= sizeof(std::uint64_t))
    {
        std::uint64_t word = 0;
        std::memcpy(&word, text.data() + index, sizeof(word));
        if ((word & high_bits) != 0)
        {
            break;
        }
        index += sizeof(word);
    }
    while (index < text.size() && static_cast<unsigned char>(text[index]) < 0x80U)
    {
        ++index;
    }
    return index;
}

[[nodiscard]] std::size_t mark_scalar(std::string_view text, std::span<std::uint64_t> words,
                                      std::size_t bit) noexcept
{
    std::size_t count = 0;
    std::size_t at = text.find('\n');
    while (at != std::string_view::npos)
    {
        place_bits(words, bit + at, 1U, 1U);
        ++count;
        at = text.find('\n', at + 1);
    }
    return count;
}

[[gnu::target("sse2")]] [[nodiscard]] std::size_t ascii_end_sse2(std::string_view text,
                                                                 std::size_t index) noexcept
{
    while (text.size() - index >= sse2_bytes)
    {
        __m128i bytes;
        std::memcpy(&bytes, text.data() + index, sse2_bytes);
        const auto high = static_cast<unsigned int>(_mm_movemask_epi8(bytes));
        if (high != 0)
        {
            return index + static_cast<std::size_t>(__builtin_ctz(high));
        }
        index += sse2_bytes;
    }
    return ascii_end_scalar(text, index);
}

[[gnu::target("sse2")]] [[nodiscard]] std::size_t
mark_sse2(std::string_view text, std::span<std::uint64_t> words, std::size_t bit) noexcept
{
    const __m128i line_feed = _mm_set1_epi8('\n');
    std::size_t count = 0;
    std::size_t index = 0;
    while (text.size() - index >= sse2_bytes)
    {
        __m128i bytes;
        std::memcpy(&bytes, text.data() + index, sse2_bytes);
        const auto found =
            static_cast<unsigned int>(_mm_movemask_epi8(_mm_cmpeq_epi8(bytes, line_feed)));
        if (found != 0)
        {
            place_bits(words, bit + index, found, sse2_bytes);
            count += static_cast<std::size_t>(__builtin_popcount(found));
        }
        index += sse2_bytes;
    }
    return count + mark_scalar(text.substr(index), words, bit + index);
}

[[gnu::target("avx2")]] [[nodiscard]] std::size_t ascii_end_avx2(std::string_view text,
                                                                 std::size_t index) noexcept
{
    while (text.size() - index >= avx2_bytes)
    {
        __m256i bytes;
        std::memcpy(&bytes, text.data() + index, avx2_bytes);
        const auto high = static_cast<unsigned int>(_mm256_movemask_epi8(bytes));
        if (high != 0)
        {
            return index + static_cast<std::size_t>(__builtin_ctz(high));
        }
        index += avx2_bytes;
    }
    return ascii_end_scalar(text, index);
}

[[gnu::target("avx2")]] [[nodiscard]] std::size_t
mark_avx2(std::string_view text, std::span<std::uint64_t> words, std::size_t bit) noexcept
{
    const __m256i line_feed = _mm256_set1_epi8('\n');
    std::size_t count = 0;
    std::size_t index = 0;
    while (text.size() - index >= avx2_bytes)
    {
        __m256i bytes;
        std::memcpy(&bytes, text.data() + index, avx2_bytes);
        const auto found =
            static_cast<unsigned int>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(bytes, line_feed)));
        if (found != 0)
        {
            place_bits(words, bit + index, found, avx2_bytes);
            count += static_cast<std::size_t>(__builtin_popcount(found));
        }
        index += avx2_bytes;
    }
    return count + mark_scalar(text.substr(index), words, bit + index);
}
} // namespace

AsciiNewlineScan select_ascii_newline_scan(const CpuFeatures &features) noexcept
{
    return features.avx2 ? AsciiNewlineScan::avx2 : AsciiNewlineScan::sse2;
}

std::size_t ascii_end(AsciiNewlineScan scan, std::string_view text, std::size_t index) noexcept
{
    switch (scan)
    {
    case AsciiNewlineScan::scalar:
        return ascii_end_scalar(text, index);
    case AsciiNewlineScan::sse2:
        return ascii_end_sse2(text, index);
    case AsciiNewlineScan::avx2:
        return ascii_end_avx2(text, index);
    }
    std::unreachable();
}

std::size_t mark_newlines(AsciiNewlineScan scan, std::string_view text,
                          std::span<std::uint64_t> words, std::size_t bit) noexcept
{
    switch (scan)
    {
    case AsciiNewlineScan::scalar:
        return mark_scalar(text, words, bit);
    case AsciiNewlineScan::sse2:
        return mark_sse2(text, words, bit);
    case AsciiNewlineScan::avx2:
        return mark_avx2(text, words, bit);
    }
    std::unreachable();
}
} // namespace nenenib::core::simd
