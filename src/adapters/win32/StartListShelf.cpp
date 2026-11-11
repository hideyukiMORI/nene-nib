#include "StartListShelf.hpp"

#include <utility>

namespace nenenib::adapters::win32
{
void StartListShelf::bind(std::function<void()> arrived)
{
    const std::scoped_lock lock(mutex_);
    arrived_ = std::move(arrived);
}

void StartListShelf::close()
{
    const std::scoped_lock lock(mutex_);
    closed_ = true;
    arrived_ = nullptr;
}

void StartListShelf::deliver(application::StartLists lists)
{
    std::function<void()> arrived;
    {
        const std::scoped_lock lock(mutex_);
        if (closed_)
        {
            return;
        }
        if (!lists_.has_value())
        {
            arrived = arrived_;
        }
        lists_ = std::move(lists);
    }
    // ロックの外で呼ぶ。合図の中から collect が呼ばれても止まらない。
    if (arrived)
    {
        arrived();
    }
}

std::optional<application::StartLists> StartListShelf::take()
{
    const std::scoped_lock lock(mutex_);
    return std::exchange(lists_, std::nullopt);
}
} // namespace nenenib::adapters::win32
