#include "AutosaveShelf.hpp"

#include <utility>

namespace nenenib::adapters::win32
{
// 手で戻す event を、仕事が無い（立っている）状態で作る。
AutosaveShelf::AutosaveShelf() : idle_(CreateEventW(nullptr, TRUE, TRUE, nullptr)) {}

void AutosaveShelf::bind(std::function<void()> failed)
{
    const std::scoped_lock lock(mutex_);
    failed_ = std::move(failed);
}

bool AutosaveShelf::open() const
{
    const std::scoped_lock lock(mutex_);
    return !closed_;
}

bool AutosaveShelf::begin()
{
    const std::scoped_lock lock(mutex_);
    if (closed_)
    {
        return false;
    }
    if (running_++ == 0 && idle_.valid())
    {
        static_cast<void>(ResetEvent(idle_.get()));
    }
    return true;
}

void AutosaveShelf::end()
{
    const std::scoped_lock lock(mutex_);
    if (--running_ == 0 && idle_.valid())
    {
        static_cast<void>(SetEvent(idle_.get()));
    }
}

void AutosaveShelf::fail(application::AutosaveFailure failure)
{
    std::function<void()> failed;
    {
        const std::scoped_lock lock(mutex_);
        if (failures_.empty())
        {
            failed = failed_;
        }
        failures_.push_back(failure);
    }
    // ロックの外で呼ぶ。合図の中から collect が呼ばれても止まらない。
    if (failed)
    {
        failed();
    }
}

std::vector<application::AutosaveFailure> AutosaveShelf::take()
{
    const std::scoped_lock lock(mutex_);
    return std::exchange(failures_, {});
}

bool AutosaveShelf::close(DWORD limit)
{
    {
        const std::scoped_lock lock(mutex_);
        closed_ = true;
        failed_ = nullptr;
    }
    // event を作れなかったときは待てないので、走っている仕事が無いかだけを答える。
    if (!idle_.valid())
    {
        const std::scoped_lock lock(mutex_);
        return running_ == 0;
    }
    return WaitForSingleObject(idle_.get(), limit) == WAIT_OBJECT_0;
}
} // namespace nenenib::adapters::win32
