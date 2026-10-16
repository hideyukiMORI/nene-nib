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

// word != 0 の最下位の立った bit の位置。
[[nodiscard]] std::size_t lowest_bit(std::uint64_t word) noexcept
{
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

[[nodiscard]] std::size_t selected_bit(std::uint64_t word, std::size_t ordinal) noexcept
{
    for (std::size_t skipped = 0; skipped < ordinal; ++skipped)
    {
        word &= word - 1;
    }
    return lowest_bit(word);
}

// 要約に、区間の中の line_begin から始まり newline の LF で終わる行を 1 つ足す。
void close_line(LineLengths &lengths, std::size_t line_begin, std::size_t newline) noexcept
{
    const std::size_t length = newline - line_begin;
    if (lengths.closed)
    {
        lengths.longest = std::max(lengths.longest, length);
        return;
    }
    lengths.head = length;
    lengths.closed = true;
}

// 区間の終わり end までの最後の行（line_begin から）を tail にした要約。'\n' が無ければ head も。
[[nodiscard]] LineLengths finished(LineLengths lengths, std::size_t line_begin,
                                   std::size_t end) noexcept
{
    lengths.tail = end - line_begin;
    if (!lengths.closed)
    {
        lengths.head = lengths.tail;
    }
    return lengths;
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
    result.blocks_.reserve(text.size() / block_bytes);
    result.append(text);
    return result;
}

void NewlineIndex::append(std::string_view text)
{
    words_.resize(word_count(size_bytes_ + text.size()), 0);
    while (!text.empty())
    {
        const auto part = text.substr(0, block_bytes - size_bytes_ % block_bytes);
        // 行の長さの要約は '\n' を探すのと同じ走査で作る（bit を読み直さない）。
        LineLengths lengths{};
        std::size_t line_begin = 0;
        std::size_t at = part.find('\n');
        while (at != std::string_view::npos)
        {
            const std::size_t offset = size_bytes_ + at;
            words_.at(offset / word_bytes) |= std::uint64_t{1} << (offset % word_bytes);
            ++newline_count_;
            close_line(lengths, line_begin, at);
            line_begin = at + 1;
            at = part.find('\n', at + 1);
        }
        pending_ = joined(pending_, finished(lengths, line_begin, part.size()));
        size_bytes_ += part.size();
        if (size_bytes_ % block_bytes == 0)
        {
            counts_.push_back(newline_count_);
            blocks_.push_back(pending_);
            pending_ = LineLengths{};
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

LineLengths NewlineIndex::scanned(std::size_t begin, std::size_t end) const noexcept
{
    LineLengths result{};
    std::size_t line_begin = begin;
    for (std::size_t word = begin / word_bytes; word * word_bytes < end; ++word)
    {
        const std::size_t base = word * word_bytes;
        auto bits = words_.at(word);
        if (base < begin)
        {
            bits &= ~((std::uint64_t{1} << (begin - base)) - 1);
        }
        if (end - base < word_bytes)
        {
            bits &= (std::uint64_t{1} << (end - base)) - 1;
        }
        for (; bits != 0; bits &= bits - 1)
        {
            const std::size_t newline = base + lowest_bit(bits);
            close_line(result, line_begin, newline);
            line_begin = newline + 1;
        }
    }
    return finished(result, line_begin, end);
}

LineLengths NewlineIndex::lengths(std::size_t begin, std::size_t end) const noexcept
{
    const std::size_t aligned =
        std::min(end, (begin + block_bytes - 1) / block_bytes * block_bytes);
    LineLengths result = scanned(begin, aligned);
    std::size_t at = aligned;
    for (; at + block_bytes <= end; at += block_bytes)
    {
        result = joined(result, blocks_.at(at / block_bytes));
    }
    return joined(result, scanned(at, end));
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
