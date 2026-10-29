#include "DocumentReadShelf.hpp"

#include <algorithm>
#include <utility>
#include <variant>

namespace nenenib::adapters::win32
{
namespace
{
[[nodiscard]] std::uint64_t ticket_of(const application::DocumentReadOutcome &outcome) noexcept
{
    return std::visit([](const auto &part) { return part.ticket; }, outcome);
}
} // namespace

void DocumentReadShelf::bind(std::function<void()> arrived)
{
    bool waiting = false;
    {
        const std::scoped_lock lock(mutex_);
        arrived_ = arrived;
        waiting = !outcomes_.empty();
    }
    if (waiting && arrived)
    {
        arrived();
    }
}

void DocumentReadShelf::start(std::uint64_t ticket)
{
    const std::scoped_lock lock(mutex_);
    running_.push_back(ticket);
}

void DocumentReadShelf::cancel(std::uint64_t ticket)
{
    const std::scoped_lock lock(mutex_);
    std::erase(running_, ticket);
    std::erase_if(outcomes_, [ticket](const application::DocumentReadOutcome &outcome)
                  { return ticket_of(outcome) == ticket; });
}

void DocumentReadShelf::close()
{
    const std::scoped_lock lock(mutex_);
    closed_ = true;
    running_.clear();
    arrived_ = nullptr;
}

bool DocumentReadShelf::wanted(std::uint64_t ticket) const
{
    const std::scoped_lock lock(mutex_);
    return running(ticket);
}

bool DocumentReadShelf::running(std::uint64_t ticket) const
{
    return !closed_ && std::ranges::find(running_, ticket) != running_.end();
}

std::function<void()> DocumentReadShelf::stored(application::DocumentReadOutcome outcome)
{
    std::function<void()> arrived;
    if (outcomes_.empty())
    {
        arrived = arrived_;
    }
    outcomes_.push_back(std::move(outcome));
    return arrived;
}

void DocumentReadShelf::progress(application::DocumentReadProgress progress)
{
    std::function<void()> arrived;
    {
        const std::scoped_lock lock(mutex_);
        if (!running(progress.ticket))
        {
            return;
        }
        arrived = stored(progress);
    }
    // ロックの外で呼ぶ。合図の中から request や collect が呼ばれても止まらない。
    if (arrived)
    {
        arrived();
    }
}

void DocumentReadShelf::finish(application::DocumentReadFinished finished)
{
    std::function<void()> arrived;
    {
        const std::scoped_lock lock(mutex_);
        if (!running(finished.ticket))
        {
            return;
        }
        std::erase(running_, finished.ticket);
        arrived = stored(std::move(finished));
    }
    if (arrived)
    {
        arrived();
    }
}

std::vector<application::DocumentReadOutcome> DocumentReadShelf::take()
{
    const std::scoped_lock lock(mutex_);
    return std::exchange(outcomes_, {});
}
} // namespace nenenib::adapters::win32
