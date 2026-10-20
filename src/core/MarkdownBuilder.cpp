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

[[nodiscard]] std::uint8_t saturated_depth(std::size_t depth) noexcept
{
    return static_cast<std::uint8_t>(depth < depth_limit ? depth : depth_limit);
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
    block.source_begin = Offset{leaf_begin_.value_or(unlocated_begin())};
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
        block.spans.at(index.value()).end = Offset{block.text.size()};
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
// 次の行頭（ADR 0151 の決定 8）。前に位置が無ければ本文の先頭。
std::size_t MarkdownBuilder::unlocated_begin() const noexcept
{
    if (!last_end_.has_value())
    {
        return 0U;
    }
    const auto newline = input_.find('\n', last_end_.value());
    return newline == std::string_view::npos ? input_.size() : newline + 1U;
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
