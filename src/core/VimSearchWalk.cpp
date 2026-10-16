#include "VimSearchWalk.hpp"

#include "LineEnding.hpp"

#include <algorithm>
#include <iterator>

namespace nenenib::core
{
namespace
{
constexpr char line_feed = '\n';
constexpr char carriage_return = '\r';
// 後ろ向きの '\n' は 1 バイトの列として探す（1 文字の rfind は core に許可されていない
// __std_find_last_trivial_1 を要求する・部分列の rfind は許可済みの __std_find_end_1）。
constexpr std::string_view line_feeds{"\n"};

// view の中で from 以降に始まる最初の literal。部分列の find は core に許可されていない
// __std_search_1 を要求するので、先頭のバイトの find（許可済み）と比較で探す。
[[nodiscard]] std::size_t literal_in(std::string_view view, std::string_view literal,
                                     std::size_t from) noexcept
{
    for (std::size_t at = view.find(literal.front(), from); at != std::string_view::npos;
         at = view.find(literal.front(), at + 1))
    {
        if (view.substr(at).starts_with(literal))
        {
            return at;
        }
    }
    return std::string_view::npos;
}
} // namespace

VimSearchWalk::VimSearchWalk(const TextBuffer &text, const VimPattern &pattern)
    : views_(text.segments()), literal_(pattern.literal_bytes()),
      crlf_(text.line_ending() == LineEnding::crlf)
{
    starts_.reserve(views_.size() + 1);
    std::size_t at = 0;
    for (const std::string_view view : views_)
    {
        starts_.push_back(at);
        at += view.size();
    }
    starts_.push_back(at);
}

std::size_t VimSearchWalk::size() const noexcept
{
    return starts_.back();
}

// at を含む view の番号。本文の末尾は最後の view（view が無ければ 0）。
std::size_t VimSearchWalk::span_at(std::size_t at) const noexcept
{
    const auto upper = std::ranges::upper_bound(starts_, at);
    const auto index = static_cast<std::size_t>(std::distance(starts_.begin(), upper));
    return std::min(index == 0 ? 0 : index - 1, views_.empty() ? 0 : views_.size() - 1);
}

std::size_t VimSearchWalk::newline_after(std::size_t at) const noexcept
{
    for (std::size_t piece = span_at(at); piece < views_.size(); ++piece)
    {
        const std::size_t base = starts_.at(piece);
        const std::size_t found = views_.at(piece).find(line_feed, at > base ? at - base : 0);
        if (found != std::string_view::npos)
        {
            return base + found;
        }
    }
    return size();
}

std::optional<std::size_t> VimSearchWalk::newline_before(std::size_t at) const noexcept
{
    if (at == 0 || views_.empty())
    {
        return std::nullopt;
    }
    for (std::size_t piece = span_at(at - 1) + 1; piece-- > 0;)
    {
        const std::size_t base = starts_.at(piece);
        const std::string_view view = views_.at(piece);
        const std::size_t found =
            view.substr(0, std::min(at - base, view.size())).rfind(line_feeds);
        if (found != std::string_view::npos)
        {
            return base + found;
        }
    }
    return std::nullopt;
}

std::size_t VimSearchWalk::line_of(std::size_t at) const noexcept
{
    const auto newline = newline_before(at);
    return newline.has_value() ? newline.value() + 1 : 0;
}

std::string_view VimSearchWalk::joined(std::size_t begin, std::size_t end)
{
    if (begin >= end)
    {
        return {};
    }
    std::size_t piece = span_at(begin);
    if (end <= starts_.at(piece + 1))
    {
        return views_.at(piece).substr(begin - starts_.at(piece), end - begin);
    }
    scratch_.clear();
    for (std::size_t at = begin; at < end; ++piece)
    {
        const std::size_t to = std::min(end, starts_.at(piece + 1));
        scratch_.append(views_.at(piece).substr(at - starts_.at(piece), to - at));
        at = to;
    }
    return scratch_;
}

std::string_view VimSearchWalk::line_at(std::size_t start)
{
    std::size_t stop = newline_after(start);
    if (crlf_ && stop > start && joined(stop - 1, stop).front() == carriage_return)
    {
        --stop;
    }
    return joined(start, stop);
}

std::optional<std::size_t> VimSearchWalk::line_after(std::size_t start) const
{
    const std::size_t newline = newline_after(start);
    return newline < size() ? std::optional<std::size_t>{newline + 1} : std::nullopt;
}

std::optional<std::size_t> VimSearchWalk::line_before(std::size_t start) const
{
    return start == 0 ? std::nullopt : std::optional<std::size_t>{line_of(start - 1)};
}

std::size_t VimSearchWalk::last_line() const
{
    return line_of(size());
}

std::optional<std::size_t> VimSearchWalk::candidate_after(std::size_t start)
{
    if (!literal_.has_value())
    {
        return start;
    }
    const auto found = literal_after(literal_.value(), start);
    return found.has_value() ? std::optional<std::size_t>{line_of(found.value())} : std::nullopt;
}

std::optional<std::size_t> VimSearchWalk::candidate_before(std::size_t start)
{
    if (!literal_.has_value())
    {
        return start;
    }
    // start の行の改行そのものに始まる候補もこの行に数える。
    const std::size_t limit = std::min(newline_after(start) + 1, size());
    const auto found = literal_before(literal_.value(), limit);
    return found.has_value() ? std::optional<std::size_t>{line_of(found.value())} : std::nullopt;
}

std::optional<std::size_t> VimSearchWalk::literal_after(std::string_view literal, std::size_t at)
{
    for (std::size_t piece = span_at(at); piece < views_.size(); ++piece)
    {
        const std::size_t base = starts_.at(piece);
        const std::size_t found = literal_in(views_.at(piece), literal, at > base ? at - base : 0);
        if (found != std::string_view::npos)
        {
            return base + found;
        }
        const auto crossing = crossing_after(literal, at, piece + 1);
        if (crossing.has_value())
        {
            return crossing;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> VimSearchWalk::literal_before(std::string_view literal,
                                                         std::size_t limit)
{
    if (limit == 0 || views_.empty())
    {
        return std::nullopt;
    }
    // 端をまたぐ候補は、その端の手前の view の中の候補より後ろにある。
    for (std::size_t piece = span_at(limit - 1) + 1; piece-- > 0;)
    {
        const auto crossing = crossing_before(literal, limit, piece + 1);
        if (crossing.has_value())
        {
            return crossing;
        }
        const std::size_t base = starts_.at(piece);
        const std::size_t found = views_.at(piece).rfind(literal, limit - 1 - base);
        if (found != std::string_view::npos)
        {
            return base + found;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> VimSearchWalk::crossing_after(std::string_view literal, std::size_t at,
                                                         std::size_t edge)
{
    if (edge >= views_.size() || literal.size() < 2)
    {
        return std::nullopt;
    }
    const std::size_t boundary = starts_.at(edge);
    const std::size_t begin = std::max(at, boundary - std::min(boundary, literal.size() - 1));
    if (begin >= boundary)
    {
        return std::nullopt;
    }
    const std::string_view window = joined(begin, std::min(size(), boundary + literal.size() - 1));
    const std::size_t found = literal_in(window, literal, 0);
    return found != std::string_view::npos && begin + found < boundary
               ? std::optional<std::size_t>{begin + found}
               : std::nullopt;
}

std::optional<std::size_t> VimSearchWalk::crossing_before(std::string_view literal,
                                                          std::size_t limit, std::size_t edge)
{
    if (edge >= views_.size() || literal.size() < 2)
    {
        return std::nullopt;
    }
    const std::size_t boundary = starts_.at(edge);
    const std::size_t begin = boundary - std::min(boundary, literal.size() - 1);
    const std::size_t stop = std::min(limit, boundary);
    if (begin >= stop)
    {
        return std::nullopt;
    }
    const std::string_view window = joined(begin, std::min(size(), boundary + literal.size() - 1));
    const std::size_t found = window.rfind(literal, stop - 1 - begin);
    return found != std::string_view::npos ? std::optional<std::size_t>{begin + found}
                                           : std::nullopt;
}
} // namespace nenenib::core
