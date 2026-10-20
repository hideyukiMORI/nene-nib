#include "AutosaveShelf.hpp"

#include <utility>

namespace nenenib::adapters::win32
{
// 手で戻す event を 2 つ、仕事が無い（立っている）状態で作る。
AutosaveShelf::AutosaveShelf()
    : idle_(CreateEventW(nullptr, TRUE, TRUE, nullptr)),
      drained_(CreateEventW(nullptr, TRUE, TRUE, nullptr))
{
}

void AutosaveShelf::bind(std::function<void()> finished)
{
    const std::scoped_lock lock(mutex_);
    finished_ = std::move(finished);
}

bool AutosaveShelf::request()
{
    const std::scoped_lock lock(mutex_);
    if (closed_)
    {
        return false;
    }
    if (requested_++ == 0 && drained_.valid())
    {
        static_cast<void>(ResetEvent(drained_.get()));
    }
    return true;
}

void AutosaveShelf::settle()
{
    const std::scoped_lock lock(mutex_);
    if (--requested_ == 0 && drained_.valid())
    {
        static_cast<void>(SetEvent(drained_.get()));
    }
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

void AutosaveShelf::finish(application::AutosaveOutcome outcome)
{
    std::function<void()> finished;
    {
        const std::scoped_lock lock(mutex_);
        finished = finished_;
        outcomes_.push_back(outcome);
    }
    // ロックの外で呼ぶ。合図の中から collect が呼ばれても止まらない。
    if (finished)
    {
        finished();
    }
}

std::vector<application::AutosaveOutcome> AutosaveShelf::take()
{
    const std::scoped_lock lock(mutex_);
    return std::exchange(outcomes_, {});
}

bool AutosaveShelf::close(DWORD limit)
{
    {
        const std::scoped_lock lock(mutex_);
        closed_ = true;
        finished_ = nullptr;
    }
    // event を作れなかったときは待てないので、走っている仕事が無いかだけを答える。
    if (!idle_.valid())
    {
        const std::scoped_lock lock(mutex_);
        return running_ == 0;
    }
    return WaitForSingleObject(idle_.get(), limit) == WAIT_OBJECT_0;
}

bool AutosaveShelf::drain(DWORD limit)
{
    // event を作れなかったときは待てないので、片付いていない仕事が無いかだけを答える。
    if (!drained_.valid())
    {
        const std::scoped_lock lock(mutex_);
        return requested_ == 0;
    }
    return WaitForSingleObject(drained_.get(), limit) == WAIT_OBJECT_0;
}
} // namespace nenenib::adapters::win32
