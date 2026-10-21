#include "MarkdownBuilder.hpp"

#include "Offset.hpp"
#include "PreviewSpan.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::string_view bullet_marker = "•";
constexpr std::size_t depth_limit = 255U;
// 画像の代わりの文字の枠（#784）。alt はこの間に入る。
constexpr std::string_view image_opening = "[画像: ";
constexpr std::string_view image_closing = "]";

[[nodiscard]] std::uint8_t saturated_depth(std::size_t depth) noexcept
{
    return static_cast<std::uint8_t>(depth < depth_limit ? depth : depth_limit);
}

// from から次の改行の位置（無ければ input.size()）。1 バイトずつ歩く（STL の検索は Debug で
// memchr を core の外へ出す・ARC-003・#784）。
[[nodiscard]] std::size_t line_end(std::string_view input, std::size_t from) noexcept
{
    while (from < input.size() && input[from] != '\n')
    {
        ++from;
    }
    return from;
}

[[nodiscard]] bool blank(char value) noexcept
{
    return value == ' ' || value == '\t' || value == '\r';
}

// [from, to) が空白（空白・Tab・CR）だけか。
[[nodiscard]] bool blank_between(std::string_view input, std::size_t from, std::size_t to) noexcept
{
    for (; from < to; ++from)
    {
        if (!blank(input[from]))
        {
            return false;
        }
    }
    return true;
}

// line から始めて、空白（空白・Tab・CR）だけの行を飛ばした最初の行の行頭。
[[nodiscard]] std::size_t first_filled_line(std::string_view input, std::size_t line) noexcept
{
    while (line < input.size())
    {
        const auto end = line_end(input, line);
        if (end == input.size() || !blank_between(input, line, end))
        {
            return line;
        }
        line = end + 1U;
    }
    return line;
}
} // namespace

MarkdownBuilder::MarkdownBuilder(std::string_view input, const MarkdownStop &stop) noexcept
    : input_(input), stop_(&stop)
{
}

bool MarkdownBuilder::should_stop()
{
    stopped_ = stopped_ || stop_->should_stop();
    return stopped_;
}

bool MarkdownBuilder::stopped() const noexcept
{
    return stopped_;
}

void MarkdownBuilder::exhaust() noexcept
{
    exhausted_ = true;
}

bool MarkdownBuilder::exhausted() const noexcept
{
    return exhausted_;
}

void MarkdownBuilder::enter_quote()
{
    close_implicit();
    ++quote_depth_;
}

void MarkdownBuilder::leave_quote()
{
    close_implicit();
    quote_depth_ -= quote_depth_ == 0U ? 0U : 1U;
}

void MarkdownBuilder::enter_list(std::optional<std::uint64_t> first)
{
    // 記号だけの項目の直後に入れ子のリストが来たら（"- - a"）、外の項目の記号を空の要素で出す。
    close_implicit();
    flush_marker();
    lists_.push_back(first);
}

void MarkdownBuilder::leave_list()
{
    close_implicit();
    if (!lists_.empty())
    {
        lists_.pop_back();
    }
}

void MarkdownBuilder::enter_item()
{
    close_implicit();
    flush_marker();
    const std::optional<std::uint64_t> number = lists_.empty() ? std::nullopt : lists_.back();
    if (!number.has_value())
    {
        marker_ = std::string(bullet_marker);
        return;
    }
    marker_ = std::to_string(number.value()) + ".";
    lists_.back() = number.value() + 1U;
}

void MarkdownBuilder::leave_item()
{
    close_implicit();
    flush_marker();
}

void MarkdownBuilder::rule()
{
    close_implicit();
    open_leaf(PreviewBlockKind::rule, 0U);
    close_leaf();
}

void MarkdownBuilder::open_leaf(PreviewBlockKind kind, std::uint8_t level)
{
    static_cast<void>(opened(kind, level));
}

void MarkdownBuilder::close_leaf()
{
    if (!leaf_.has_value())
    {
        return;
    }
    auto block = std::move(leaf_).value();
    leaf_.reset();
    // 行の探索は位置を持たない要素のときだけ走らせる（value_or は先に評価してしまう）。
    if (leaf_begin_.has_value())
    {
        block.source_begin = Offset{leaf_begin_.value()};
    }
    else
    {
        block.source_begin = Offset{unlocated_begin()};
    }
    blocks_.push_back(std::move(block));
    leaf_begin_.reset();
    implicit_leaf_ = false;
    spans_.clear();
}

void MarkdownBuilder::enter_span(std::optional<PreviewStyle> style)
{
    auto &block = ensure_leaf();
    if (!style.has_value())
    {
        spans_.emplace_back(std::nullopt);
        return;
    }
    const Offset at{block.text.size()};
    spans_.emplace_back(block.spans.size());
    block.spans.push_back(PreviewSpan{at, at, style.value()});
    if (style.value() == PreviewStyle::image)
    {
        block.text.append(image_opening);
    }
}

void MarkdownBuilder::leave_span()
{
    if (spans_.empty())
    {
        return;
    }
    const auto index = spans_.back();
    spans_.pop_back();
    if (index.has_value() && leaf_.has_value())
    {
        auto &block = leaf_.value();
        auto &span = block.spans.at(index.value());
        if (span.style == PreviewStyle::image)
        {
            block.text.append(image_closing);
        }
        span.end = Offset{block.text.size()};
    }
}

void MarkdownBuilder::text(std::string_view shown, std::string_view source)
{
    ensure_leaf().text.append(shown);
    const auto at = located(source);
    if (!at.has_value())
    {
        return;
    }
    if (!leaf_begin_.has_value())
    {
        leaf_begin_ = at;
    }
    last_end_ = at.value() + source.size();
}

MarkdownDocument MarkdownBuilder::finished() &&
{
    close_implicit();
    flush_marker();
    close_leaf();
    return MarkdownDocument{std::move(blocks_)};
}

std::optional<std::size_t> MarkdownBuilder::located(std::string_view source) const
{
    // md4c の固定の文字列（改行・字下げ）は本文の外にある。別の配列の pointer の大小は
    // std::less_equal の全順序で比べる。
    const std::less_equal<const char *> not_after;
    if (source.empty() || !not_after(input_.data(), source.data()) ||
        !not_after(source.data() + source.size(), input_.data() + input_.size()))
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(source.data() - input_.data());
}

// 文字の位置を持たない要素（水平線・空の項目・空のコードブロック）は、直前の要素の終わりの
// 次の行から空行を飛ばした最初の行の行頭（ADR 0151 の決定 8 の追記）。前に位置が無ければ
// 本文の先頭から空行を飛ばす。
std::size_t MarkdownBuilder::unlocated_begin() const noexcept
{
    std::size_t line = 0U;
    if (last_end_.has_value())
    {
        const auto newline = line_end(input_, last_end_.value());
        line = newline == input_.size() ? input_.size() : newline + 1U;
    }
    return first_filled_line(input_, line);
}

PreviewBlock MarkdownBuilder::started(PreviewBlockKind kind, std::uint8_t level)
{
    const bool first_of_item = marker_.has_value();
    return PreviewBlock{
        .kind = first_of_item && kind == PreviewBlockKind::paragraph ? PreviewBlockKind::list_item
                                                                     : kind,
        .level = level,
        .quote_depth = saturated_depth(quote_depth_),
        .list_depth = saturated_depth(lists_.size()),
        .marker = std::exchange(marker_, std::nullopt).value_or(std::string{}),
        .source_begin = Offset{0U},
        .text = {},
        .spans = {},
    };
}

PreviewBlock &MarkdownBuilder::opened(PreviewBlockKind kind, std::uint8_t level)
{
    close_implicit();
    leaf_begin_.reset();
    implicit_leaf_ = false;
    return leaf_.emplace(started(kind, level));
}

PreviewBlock &MarkdownBuilder::ensure_leaf()
{
    if (leaf_.has_value())
    {
        return leaf_.value();
    }
    auto &block = opened(PreviewBlockKind::paragraph, 0U);
    implicit_leaf_ = true;
    return block;
}

void MarkdownBuilder::close_implicit()
{
    if (implicit_leaf_)
    {
        close_leaf();
    }
}

void MarkdownBuilder::flush_marker()
{
    if (!marker_.has_value())
    {
        return;
    }
    open_leaf(PreviewBlockKind::list_item, 0U);
    close_leaf();
}
} // namespace nenenib::core
