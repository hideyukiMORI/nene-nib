#include "MarkdownShelf.hpp"

#include <utility>

namespace nenenib::adapters::win32
{
void MarkdownShelf::bind(std::function<void()> arrived)
{
    const std::scoped_lock lock(mutex_);
    arrived_ = std::move(arrived);
}

void MarkdownShelf::renew(std::uint64_t ticket) noexcept
{
    latest_ticket_.store(ticket);
}

void MarkdownShelf::close()
{
    closed_.store(true);
    const std::scoped_lock lock(mutex_);
    arrived_ = nullptr;
}

bool MarkdownShelf::wanted(std::uint64_t ticket) const noexcept
{
    return !closed_.load() && latest_ticket_.load() == ticket;
}

void MarkdownShelf::deliver(application::MarkdownOutcome outcome)
{
    std::function<void()> arrived;
    {
        const std::scoped_lock lock(mutex_);
        if (closed_.load())
        {
            return;
        }
        if (outcomes_.empty())
        {
            arrived = arrived_;
            outcomes_.push_back(std::move(outcome));
        }
        else
        {
            outcomes_.front() = std::move(outcome);
        }
    }
    // ロックの外で呼ぶ。合図の中から parse や collect が呼ばれても止まらない。
    if (arrived)
    {
        arrived();
    }
}

std::vector<application::MarkdownOutcome> MarkdownShelf::take()
{
    const std::scoped_lock lock(mutex_);
    return std::exchange(outcomes_, {});
}
} // namespace nenenib::adapters::win32
