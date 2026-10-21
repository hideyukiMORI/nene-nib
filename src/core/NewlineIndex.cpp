#include "NewlineIndex.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iterator>
#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
constexpr std::size_t word_bytes = 64;
constexpr std::size_t block_bytes = 512;
constexpr std::size_t block_words = block_bytes / word_bytes;
// 塊は 64 KiB（VirtualColumnIndex と同じ刻み・ADR 0121 の追補の決定 2）。
constexpr std::size_t chunk_shift = 16;
constexpr std::size_t chunk_bytes = std::size_t{1} << chunk_shift;
constexpr std::size_t chunk_words = chunk_bytes / word_bytes;
constexpr std::size_t chunk_blocks = chunk_bytes / block_bytes;
// 疎の 2 byte × 個数が密の 8 KiB と等しくなる所で密にする（決定 3・4・10）。
constexpr std::size_t dense_newlines = 4096;

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

// 密の塊の 512 byte の区切り (settled, last] に「その区切りの前まで」の数 total を書く。追記は
// 前からだけなので、書く区切りの前の LF はちょうど total 個ある。
void settle(NewlineDenseChunk &dense, std::size_t settled, std::size_t last,
            std::size_t total) noexcept
{
    for (std::size_t block = settled + 1; block <= last; ++block)
    {
        dense.counts.at(block) = static_cast<std::uint16_t>(total);
    }
}

void set_bit(NewlineDenseChunk &dense, std::size_t local) noexcept
{
    dense.words.at(local / word_bytes) |= std::uint64_t{1} << (local % word_bytes);
}

// 疎の塊を密に写す（4,096 個目の LF を足した直後に 1 回・決定 6）。区切りの累積は最後の LF の
// 区切りまで書く。
[[nodiscard]] NewlineDenseChunk densified(const NewlineSparseChunk &sparse)
{
    NewlineDenseChunk dense{std::vector<std::uint64_t>(chunk_words, 0),
                            std::vector<std::uint16_t>(chunk_blocks, 0)};
    std::size_t settled = 0;
    for (std::size_t ordinal = 0; ordinal < sparse.positions.size(); ++ordinal)
    {
        const std::size_t local = sparse.positions.at(ordinal);
        settle(dense, settled, local / block_bytes, ordinal);
        settled = std::max(settled, local / block_bytes);
        set_bit(dense, local);
    }
    return dense;
}

// 塊の local に LF を 1 つ足す。total はそれより前の塊の中の LF の数、settled は累積を書いた最後の
// 区切り。疎の塊が 4,096 個に達したら true（呼び出し元が密に替える）。
[[nodiscard]] bool added(NewlineSparseChunk &sparse, std::size_t local, std::size_t total,
                         std::size_t settled)
{
    static_cast<void>(total);
    static_cast<void>(settled);
    sparse.positions.push_back(static_cast<std::uint16_t>(local));
    return sparse.positions.size() == dense_newlines;
}

[[nodiscard]] bool added(NewlineDenseChunk &dense, std::size_t local, std::size_t total,
                         std::size_t settled) noexcept
{
    settle(dense, settled, local / block_bytes, total);
    set_bit(dense, local);
    return false;
}

// 書いたバイトの最後の区切り last まで累積を書く（疎の塊には区切りの累積が無い）。
void settled_to(NewlineSparseChunk &sparse, std::size_t settled, std::size_t last,
                std::size_t total) noexcept
{
    static_cast<void>(sparse);
    static_cast<void>(settled);
    static_cast<void>(last);
    static_cast<void>(total);
}

void settled_to(NewlineDenseChunk &dense, std::size_t settled, std::size_t last,
                std::size_t total) noexcept
{
    settle(dense, settled, last, total);
}

// 書き終えた塊は不変なので、疎の列の余りの容量を返す（密は大きさが決まっている）。
void sealed(NewlineSparseChunk &sparse)
{
    sparse.positions.shrink_to_fit();
}

void sealed(NewlineDenseChunk &dense) noexcept
{
    static_cast<void>(dense);
}

void seal(NewlineChunk &chunk)
{
    std::visit([](auto &held) { sealed(held); }, chunk);
}

// 塊の中の local より前の LF の数。
[[nodiscard]] std::size_t counted_before(const NewlineSparseChunk &sparse,
                                         std::size_t local) noexcept
{
    const auto bound = std::ranges::lower_bound(sparse.positions, local);
    return static_cast<std::size_t>(bound - sparse.positions.begin());
}

// 区切りの累積と最大 8 word の bit の数（決定 5）。
[[nodiscard]] std::size_t counted_before(const NewlineDenseChunk &dense, std::size_t local) noexcept
{
    const std::size_t block = local / block_bytes;
    std::size_t count = dense.counts.at(block);
    const std::size_t stop = local / word_bytes;
    for (std::size_t word = block * block_words; word < stop; ++word)
    {
        count += counted_bits(dense.words.at(word));
    }
    const std::size_t remainder = local % word_bytes;
    if (remainder != 0)
    {
        const auto mask = (std::uint64_t{1} << remainder) - 1;
        count += counted_bits(dense.words.at(stop) & mask);
    }
    return count;
}

// 塊の中の ordinal 番目の LF の位置。written は塊の書いたバイト数（区切りの累積が正しい範囲）。
[[nodiscard]] std::size_t selected(const NewlineSparseChunk &sparse, std::size_t ordinal,
                                   std::size_t written) noexcept
{
    static_cast<void>(written);
    return sparse.positions.at(ordinal);
}

// 区切りの累積の upper_bound と最大 8 word の走査（決定 5）。
[[nodiscard]] std::size_t selected(const NewlineDenseChunk &dense, std::size_t ordinal,
                                   std::size_t written) noexcept
{
    const auto settled = static_cast<std::ptrdiff_t>(((written - 1) / block_bytes) + 1);
    const auto bound =
        std::upper_bound(dense.counts.begin(), std::next(dense.counts.begin(), settled), ordinal);
    const auto block = static_cast<std::size_t>(bound - dense.counts.begin()) - 1;
    std::size_t remaining = ordinal - dense.counts.at(block);
    const std::size_t stop = std::min((block + 1) * block_words, chunk_words);
    for (std::size_t word = block * block_words; word < stop; ++word)
    {
        const auto bits = dense.words.at(word);
        const auto count = counted_bits(bits);
        if (remaining < count)
        {
            return (word * word_bytes) + selected_bit(bits, remaining);
        }
        remaining -= count;
    }
    std::unreachable();
}

// 塊の中の [begin, end) の要約を LF の位置の列から数える。
[[nodiscard]] LineLengths scanned_lines(const NewlineSparseChunk &sparse, std::size_t begin,
                                        std::size_t end) noexcept
{
    LineLengths result{};
    std::size_t line_begin = begin;
    for (auto at = std::ranges::lower_bound(sparse.positions, begin);
         at != sparse.positions.end() && *at < end; ++at)
    {
        close_line(result, line_begin, *at);
        line_begin = std::size_t{*at} + 1;
    }
    return finished(result, line_begin, end);
}

// 塊の中の [begin, end) の要約を bit から数える。読む word は区間に比例する。
[[nodiscard]] LineLengths scanned_lines(const NewlineDenseChunk &dense, std::size_t begin,
                                        std::size_t end) noexcept
{
    LineLengths result{};
    std::size_t line_begin = begin;
    for (std::size_t word = begin / word_bytes; word * word_bytes < end; ++word)
    {
        const std::size_t base = word * word_bytes;
        auto bits = dense.words.at(word);
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
} // namespace

NewlineIndex NewlineIndex::empty()
{
    return NewlineIndex();
}

NewlineIndex NewlineIndex::from_text(std::string_view text)
{
    auto result = empty();
    const std::size_t chunks = (text.size() + chunk_bytes - 1) / chunk_bytes;
    result.counts_.reserve(chunks);
    result.chunks_.reserve(chunks);
    result.lengths_.reserve(chunks);
    result.append(text);
    if (!result.chunks_.empty())
    {
        seal(result.chunks_.back());
    }
    return result;
}

void NewlineIndex::append(std::string_view text)
{
    while (!text.empty())
    {
        const std::size_t local = size_bytes_ % chunk_bytes;
        if (local == 0)
        {
            counts_.push_back(newline_count_);
            chunks_.emplace_back(NewlineSparseChunk{});
            lengths_.emplace_back();
        }
        const auto part = text.substr(0, chunk_bytes - local);
        append_part(part, local);
        size_bytes_ += part.size();
        if (size_bytes_ % chunk_bytes == 0)
        {
            seal(chunks_.back());
        }
        text.remove_prefix(part.size());
    }
}

void NewlineIndex::append_part(std::string_view part, std::size_t local)
{
    // 行の長さの要約は '\n' を探すのと同じ走査で作る（表現を読み直さない）。
    LineLengths lengths{};
    std::size_t line_begin = 0;
    std::size_t settled = local == 0 ? 0 : (local - 1) / block_bytes;
    std::size_t total = newline_count_ - counts_.back();
    NewlineChunk &chunk = chunks_.back();
    for (std::size_t at = part.find('\n'); at != std::string_view::npos;
         at = part.find('\n', at + 1))
    {
        const std::size_t position = local + at;
        const bool full = std::visit([position, total, settled](auto &held)
                                     { return added(held, position, total, settled); }, chunk);
        if (full)
        {
            chunk = densified(std::get<NewlineSparseChunk>(chunk));
        }
        settled = std::max(settled, position / block_bytes);
        ++total;
        close_line(lengths, line_begin, at);
        line_begin = at + 1;
    }
    const std::size_t last = (local + part.size() - 1) / block_bytes;
    std::visit([settled, last, total](auto &held) { settled_to(held, settled, last, total); },
               chunk);
    newline_count_ = counts_.back() + total;
    lengths_.back() = joined(lengths_.back(), finished(lengths, line_begin, part.size()));
}

std::size_t NewlineIndex::size() const noexcept
{
    return newline_count_;
}

std::size_t NewlineIndex::count_before(std::size_t offset) const noexcept
{
    if (offset >= size_bytes_)
    {
        return newline_count_;
    }
    const std::size_t local = offset % chunk_bytes;
    return counts_.at(offset >> chunk_shift) + std::visit([local](const auto &chunk) noexcept
                                                          { return counted_before(chunk, local); },
                                                          chunks_.at(offset >> chunk_shift));
}

LineLengths NewlineIndex::scanned(std::size_t begin, std::size_t end) const noexcept
{
    if (begin == end)
    {
        return LineLengths{};
    }
    const std::size_t base = begin / chunk_bytes * chunk_bytes;
    return std::visit([begin, end, base](const auto &chunk) noexcept
                      { return scanned_lines(chunk, begin - base, end - base); },
                      chunks_.at(begin >> chunk_shift));
}

LineLengths NewlineIndex::lengths(std::size_t begin, std::size_t end) const noexcept
{
    const std::size_t aligned =
        std::min(end, (begin + chunk_bytes - 1) / chunk_bytes * chunk_bytes);
    LineLengths result = scanned(begin, aligned);
    std::size_t at = aligned;
    for (; at + chunk_bytes <= end; at += chunk_bytes)
    {
        result = joined(result, lengths_.at(at >> chunk_shift));
    }
    return joined(result, scanned(at, end));
}

Offset NewlineIndex::at(std::size_t ordinal) const noexcept
{
    const auto bound = std::ranges::upper_bound(counts_, ordinal);
    const auto chunk = static_cast<std::size_t>(bound - counts_.begin()) - 1;
    const std::size_t base = chunk << chunk_shift;
    const std::size_t written = std::min(chunk_bytes, size_bytes_ - base);
    const std::size_t remaining = ordinal - counts_.at(chunk);
    return Offset{base + std::visit([remaining, written](const auto &held) noexcept
                                    { return selected(held, remaining, written); },
                                    chunks_.at(chunk))};
}
} // namespace nenenib::core
