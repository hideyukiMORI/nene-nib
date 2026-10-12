#include "NewlineIndex.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::size_t word_bytes = 64;
constexpr std::size_t block_bytes = 512;
constexpr std::size_t block_words = block_bytes / word_bytes;

// MSVC STLの実行時bit関数はCPU判定を読むため、標準関数は定数表の生成にだけ使う。
constexpr auto bit_counts = []
{
    std::array<std::size_t, 256> result{};
    for (std::size_t byte = 0; byte < result.size(); ++byte)
    {
        result.at(byte) = static_cast<std::size_t>(std::popcount(byte));
    }
    return result;
}();

constexpr auto first_bits = []
{
    std::array<std::size_t, 256> result{};
    for (std::size_t byte = 0; byte < result.size(); ++byte)
    {
        result.at(byte) = static_cast<std::size_t>(std::countr_zero(byte));
    }
    return result;
}();

[[nodiscard]] std::size_t counted_bits(std::uint64_t word) noexcept
{
    std::size_t count = 0;
    for (std::size_t byte = 0; byte < sizeof(word); ++byte)
    {
        count += bit_counts.at(word & 0xffU);
        word >>= 8;
    }
    return count;
}

[[nodiscard]] std::size_t word_count(std::size_t bytes) noexcept
{
    return bytes / word_bytes + (bytes % word_bytes != 0 ? 1U : 0U);
}

[[nodiscard]] std::size_t selected_bit(std::uint64_t word, std::size_t ordinal) noexcept
{
    for (std::size_t skipped = 0; skipped < ordinal; ++skipped)
    {
        word &= word - 1;
    }
    for (std::size_t byte = 0; byte < sizeof(word); ++byte)
    {
        const auto bits = word & 0xffU;
        if (bits != 0)
        {
            return byte * 8 + first_bits.at(bits);
        }
        word >>= 8;
    }
    std::unreachable();
}
} // namespace

NewlineIndex NewlineIndex::empty()
{
    return NewlineIndex();
}

NewlineIndex NewlineIndex::from_text(std::string_view text)
{
    auto result = empty();
    result.words_.reserve(word_count(text.size()));
    result.counts_.reserve(text.size() / block_bytes);
    result.append(text);
    return result;
}

void NewlineIndex::append(std::string_view text)
{
    words_.resize(word_count(size_bytes_ + text.size()), 0);
    while (!text.empty())
    {
        const auto part = text.substr(0, block_bytes - size_bytes_ % block_bytes);
        std::size_t at = part.find('\n');
        while (at != std::string_view::npos)
        {
            const std::size_t offset = size_bytes_ + at;
            words_.at(offset / word_bytes) |= std::uint64_t{1} << (offset % word_bytes);
            ++newline_count_;
            at = part.find('\n', at + 1);
        }
        size_bytes_ += part.size();
        if (size_bytes_ % block_bytes == 0)
        {
            counts_.push_back(newline_count_);
        }
        text.remove_prefix(part.size());
    }
}

std::size_t NewlineIndex::size() const noexcept
{
    return newline_count_;
}

std::size_t NewlineIndex::count_before(std::size_t offset) const noexcept
{
    const std::size_t limit = std::min(offset, size_bytes_);
    const std::size_t block = limit / block_bytes;
    std::size_t count = block == 0 ? 0 : counts_.at(block - 1);
    const std::size_t stop = limit / word_bytes;
    for (std::size_t word = block * block_words; word < stop; ++word)
    {
        count += counted_bits(words_.at(word));
    }
    const std::size_t remainder = limit % word_bytes;
    if (remainder != 0)
    {
        const auto mask = (std::uint64_t{1} << remainder) - 1;
        count += counted_bits(words_.at(stop) & mask);
    }
    return count;
}

Offset NewlineIndex::at(std::size_t ordinal) const noexcept
{
    const auto bound = std::ranges::upper_bound(counts_, ordinal);
    const auto block = static_cast<std::size_t>(bound - counts_.begin());
    std::size_t remaining = ordinal - (block == 0 ? 0 : counts_.at(block - 1));
    const std::size_t stop = std::min((block + 1) * block_words, words_.size());
    for (std::size_t word = block * block_words; word < stop; ++word)
    {
        const auto bits = words_.at(word);
        const auto count = counted_bits(bits);
        if (remaining < count)
        {
            return Offset{word * word_bytes + selected_bit(bits, remaining)};
        }
        remaining -= count;
    }
    std::unreachable();
}
} // namespace nenenib::core
