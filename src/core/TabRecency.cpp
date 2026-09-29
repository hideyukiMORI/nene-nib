#include "TabRecency.hpp"

#include <iterator>
#include <utility>

namespace nenenib::core
{
namespace
{
// 列の中の tab の添字。無ければ値なし。
[[nodiscard]] std::optional<std::size_t> index_of(std::span<const std::size_t> order,
                                                  std::size_t tab) noexcept
{
    for (std::size_t index = 0; index < order.size(); ++index)
    {
        if (order[index] == tab)
        {
            return index;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::ptrdiff_t distance_of(std::size_t index) noexcept
{
    return static_cast<std::ptrdiff_t>(index);
}
} // namespace

TabRecency::TabRecency(std::vector<std::size_t> order) : order_(std::move(order)) {}

TabRecency TabRecency::single()
{
    return TabRecency(std::vector<std::size_t>{0});
}

std::span<const std::size_t> TabRecency::order() const & noexcept
{
    return order_;
}

TabRecency tab_recency_touched(const TabRecency &recency, std::size_t tab)
{
    // 列は全部の帯の位置を 1 回ずつ含むので、範囲の中の位置は必ず列にある。
    const auto index = index_of(recency.order_, tab);
    if (!index.has_value())
    {
        return recency;
    }
    std::vector<std::size_t> order = recency.order_;
    order.erase(std::next(order.begin(), distance_of(index.value())));
    order.insert(order.begin(), tab);
    return TabRecency(std::move(order));
}

TabRecency tab_recency_opened(const TabRecency &recency, std::size_t tab)
{
    if (tab > recency.order_.size())
    {
        return recency;
    }
    std::vector<std::size_t> order;
    order.reserve(recency.order_.size() + 1);
    order.push_back(tab);
    for (const std::size_t position : recency.order_)
    {
        order.push_back(position >= tab ? position + 1 : position);
    }
    return TabRecency(std::move(order));
}

TabRecency tab_recency_closed(const TabRecency &recency, std::size_t tab)
{
    if (tab >= recency.order_.size() || recency.order_.size() == 1)
    {
        return recency;
    }
    std::vector<std::size_t> order;
    order.reserve(recency.order_.size() - 1);
    for (const std::size_t position : recency.order_)
    {
        if (position != tab)
        {
            order.push_back(position > tab ? position - 1 : position);
        }
    }
    return TabRecency(std::move(order));
}

std::optional<std::size_t> tab_recency_walked(const TabRecency &recency, std::size_t from,
                                              TabStep step)
{
    const auto order = recency.order();
    const auto index = index_of(order, from);
    if (!index.has_value())
    {
        return std::nullopt;
    }
    const std::size_t count = order.size();
    switch (step)
    {
    case TabStep::next:
        return order[(index.value() + 1) % count];
    case TabStep::previous:
        return order[(index.value() + count - 1) % count];
    }
    std::unreachable();
}
} // namespace nenenib::core
