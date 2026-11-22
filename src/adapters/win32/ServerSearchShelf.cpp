#include "ServerSearchShelf.hpp"

#include <utility>

namespace nenenib::adapters::win32
{
void ServerSearchShelf::bind(std::function<void()> arrived)
{
    const std::scoped_lock lock(mutex_);
    arrived_ = std::move(arrived);
}

void ServerSearchShelf::close()
{
    const std::scoped_lock lock(mutex_);
    closed_ = true;
    arrived_ = nullptr;
}

void ServerSearchShelf::deliver(Found found)
{
    std::function<void()> arrived;
    {
        const std::scoped_lock lock(mutex_);
        if (closed_)
        {
            return;
        }
        if (!found_.has_value())
        {
            arrived = arrived_;
        }
        found_ = std::move(found);
    }
    // ロックの外で呼ぶ。合図の中から collect が呼ばれても止まらない。
    if (arrived)
    {
        arrived();
    }
}

std::optional<ServerSearchShelf::Found> ServerSearchShelf::take()
{
    const std::scoped_lock lock(mutex_);
    return std::exchange(found_, std::nullopt);
}
} // namespace nenenib::adapters::win32
