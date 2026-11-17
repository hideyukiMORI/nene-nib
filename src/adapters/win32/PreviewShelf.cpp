#include "PreviewShelf.hpp"

#include <utility>

namespace nenenib::adapters::win32
{
void PreviewShelf::bind(std::function<void()> arrived)
{
    const std::scoped_lock lock(mutex_);
    arrived_ = std::move(arrived);
}

std::uint64_t PreviewShelf::renew(std::uint64_t ticket)
{
    const std::scoped_lock lock(mutex_);
    ticket_ = ticket;
    return ++generation_;
}

void PreviewShelf::cancel(std::uint64_t ticket)
{
    const std::scoped_lock lock(mutex_);
    if (ticket == ticket_)
    {
        ++generation_;
    }
}

void PreviewShelf::close()
{
    const std::scoped_lock lock(mutex_);
    ++generation_;
    arrived_ = nullptr;
}

bool PreviewShelf::current(std::uint64_t generation) const
{
    const std::scoped_lock lock(mutex_);
    return generation == generation_;
}

bool PreviewShelf::deliver(std::uint64_t generation, application::PreviewBatch batch)
{
    std::function<void()> arrived;
    {
        const std::scoped_lock lock(mutex_);
        if (generation != generation_)
        {
            return false;
        }
        if (batches_.empty())
        {
            arrived = arrived_;
        }
        batches_.push_back(std::move(batch));
    }
    // ロックの外で呼ぶ。合図の中から request や collect が呼ばれても止まらない。
    if (arrived)
    {
        arrived();
    }
    return true;
}

std::vector<application::PreviewBatch> PreviewShelf::take()
{
    const std::scoped_lock lock(mutex_);
    return std::exchange(batches_, {});
}
} // namespace nenenib::adapters::win32
