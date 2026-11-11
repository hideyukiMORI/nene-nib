#include "ImageShelf.hpp"

#include <utility>

namespace nenenib::adapters::win32
{
void ImageShelf::bind(std::function<void()> arrived)
{
    auto shared = std::make_shared<const std::function<void()>>(std::move(arrived));
    const std::scoped_lock lock(mutex_);
    arrived_ = std::move(shared);
}

void ImageShelf::reserve()
{
    const std::scoped_lock lock(mutex_);
    outcomes_.reserve(outcomes_.size() + reserved_ + 1U);
    ++reserved_;
}

void ImageShelf::close()
{
    const std::scoped_lock lock(mutex_);
    closed_ = true;
    arrived_.reset();
}

bool ImageShelf::closed() const
{
    const std::scoped_lock lock(mutex_);
    return closed_;
}

void ImageShelf::deliver(application::ImageOutcome outcome) noexcept
{
    std::shared_ptr<const std::function<void()>> arrived;
    {
        const std::scoped_lock lock(mutex_);
        if (closed_ || reserved_ == 0U)
        {
            return;
        }
        --reserved_;
        if (outcomes_.empty())
        {
            arrived = arrived_;
        }
        // reserve で席を取ってあるので、ここで記憶を取らない。
        outcomes_.push_back(std::move(outcome));
    }
    // ロックの外で呼ぶ。合図の中から request や collect が呼ばれても止まらない。
    if (arrived != nullptr && *arrived)
    {
        (*arrived)();
    }
}

std::vector<application::ImageOutcome> ImageShelf::take()
{
    const std::scoped_lock lock(mutex_);
    std::vector<application::ImageOutcome> taken;
    taken.reserve(outcomes_.size());
    for (application::ImageOutcome &outcome : outcomes_)
    {
        taken.push_back(std::move(outcome));
    }
    // clear は capacity を残すので、取ってある席はそのまま使える。
    outcomes_.clear();
    return taken;
}
} // namespace nenenib::adapters::win32
