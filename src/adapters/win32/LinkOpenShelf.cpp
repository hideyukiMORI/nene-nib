#include "LinkOpenShelf.hpp"

#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace nenenib::adapters::win32
{
void LinkOpenShelf::bind(std::function<void()> arrived)
{
    const std::scoped_lock lock(mutex_);
    arrived_ = std::move(arrived);
}

void LinkOpenShelf::close()
{
    const std::scoped_lock lock(mutex_);
    closed_ = true;
    arrived_ = nullptr;
}

void LinkOpenShelf::deliver(Result result)
{
    std::function<void()> arrived;
    {
        const std::scoped_lock lock(mutex_);
        if (closed_)
        {
            return;
        }
        if (results_.empty())
        {
            arrived = arrived_;
        }
        results_.push_back(result);
    }
    // ロックの外で呼ぶ。合図の中から collect が呼ばれても止まらない。
    if (arrived)
    {
        arrived();
    }
}

std::vector<LinkOpenShelf::Result> LinkOpenShelf::take()
{
    const std::scoped_lock lock(mutex_);
    return std::exchange(results_, {});
}
} // namespace nenenib::adapters::win32
