#include "GiantLine.hpp"

#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"
#include "VirtualColumnIndex.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
// Vim の 1 文字を確かめるために最初に読む前後のバイト数。足りなければ倍にして読み直す。
constexpr std::size_t first_margin = 64;
constexpr unsigned char continuation_mask = 0xC0;
constexpr unsigned char continuation_bits = 0x80;

[[nodiscard]] std::size_t leading_continuations(std::string_view bytes) noexcept
{
    std::size_t count = 0;
    while (count < bytes.size() &&
           (static_cast<unsigned char>(bytes[count]) & continuation_mask) == continuation_bits)
    {
        ++count;
    }
    return count;
}

[[nodiscard]] std::size_t code_point_floor(std::string_view bytes, std::size_t at) noexcept
{
    while (at > 0 && !is_boundary(bytes, Offset{at}))
    {
        --at;
    }
    return at;
}

// at の前後 margin バイトを読み、at を含む Vim の 1 文字を決める。文字が読んだ範囲の端に触れて
// いて（行の端は除く）決められなければ空。
[[nodiscard]] std::optional<OffsetRange> read_around(const TextBuffer &text, OffsetRange line,
                                                     std::size_t at, std::size_t margin)
{
    const auto from = at - std::min(at - line.begin.value, margin);
    const auto to = std::min(line.end.value, at + margin);
    const auto read = text.text_range(Offset{from}, Offset{to});
    const auto skip = leading_continuations(read);
    const auto bytes = std::string_view(read).substr(skip);
    const auto base = from + skip;
    if (at < base)
    {
        return std::nullopt;
    }
    const auto rel = code_point_floor(bytes, at - base);
    if (base + rel == line.begin.value)
    {
        return OffsetRange{line.begin, line.begin};
    }
    const auto start = vim_character_start(bytes, Offset{rel}).value;
    const auto end = vim_character_end(bytes, Offset{start}).value;
    const bool cut_left = start == 0 && base > line.begin.value;
    const bool cut_right = end == bytes.size() && to < line.end.value;
    if (cut_left || cut_right)
    {
        return std::nullopt;
    }
    return end == rel ? OffsetRange{Offset{base + rel}, Offset{base + rel}}
                      : OffsetRange{Offset{base + start}, Offset{base + end}};
}

// at（行の中のバイト位置）を含む Vim の 1 文字 [begin, end)。at が code point の途中なら先頭へ
// 寄せ、そこが Vim の 1 文字の境目なら空の区間 [at, at)。本文は at の前後だけを読み、文字が
// 読んだ範囲の端に触れていれば倍の範囲で読み直す（行の端までで必ず決まる）。
[[nodiscard]] OffsetRange character_around(const TextBuffer &text, OffsetRange line, std::size_t at)
{
    if (at <= line.begin.value || at >= line.end.value)
    {
        return OffsetRange{Offset{at}, Offset{at}};
    }
    for (std::size_t margin = first_margin;; margin *= 2)
    {
        const auto found = read_around(text, line, at, margin);
        if (found.has_value())
        {
            return found.value();
        }
    }
}
} // namespace

OffsetRange giant_line_window(const VirtualColumnIndex &index, const TextBuffer &text,
                              LineNumber line, Offset anchor)
{
    const OffsetRange range{text.line_start(line), text.line_end(line)};
    const auto clamped = std::clamp(anchor.value, range.begin.value, range.end.value);
    const auto at = character_around(text, range, clamped).begin;
    const auto ordinal = index.code_point_at(text, at);
    const auto center = (ordinal / giant_line_step) * giant_line_step;
    const auto first = center - std::min(center, giant_line_half_width);
    const auto last = center + giant_line_half_width;
    const auto begin = index.byte_at_code_point(text, line, first);
    const auto end = index.byte_at_code_point(text, line, last);
    return OffsetRange{character_around(text, range, begin.value).begin,
                       character_around(text, range, end.value).end};
}
} // namespace nenenib::core
