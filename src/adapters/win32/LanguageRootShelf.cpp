#include "LanguageRootShelf.hpp"

#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace nenenib::adapters::win32
{
void LanguageRootShelf::bind(std::function<void()> arrived)
{
    const std::scoped_lock lock(mutex_);
    arrived_ = std::move(arrived);
}

void LanguageRootShelf::close()
{
    const std::scoped_lock lock(mutex_);
    closed_ = true;
    arrived_ = nullptr;
}

void LanguageRootShelf::deliver(application::LanguageRootAnswer answer)
{
    std::function<void()> arrived;
    {
        const std::scoped_lock lock(mutex_);
        if (closed_)
        {
            return;
        }
        if (answers_.empty())
        {
            arrived = arrived_;
        }
        answers_.push_back(std::move(answer));
    }
    // ロックの外で呼ぶ。合図の中から collect が呼ばれても止まらない。
    if (arrived)
    {
        arrived();
    }
}

std::vector<application::LanguageRootAnswer> LanguageRootShelf::take()
{
    const std::scoped_lock lock(mutex_);
    return std::exchange(answers_, {});
}
} // namespace nenenib::adapters::win32
