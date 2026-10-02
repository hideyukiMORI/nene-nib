#include "Win32Worker.hpp"

#include <windows.h>

#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
// CancelSynchronousIo は呼んだ瞬間に進んでいる I/O しか取り消さないので、スレッドが終わるまで
// 「取り消す → この長さだけスレッドのハンドルを待つ」を繰り返す。時刻は読まない。
constexpr DWORD cancel_wait_milliseconds = 10;

void cancel_until_ended(HANDLE thread) noexcept
{
    while (true)
    {
        CancelSynchronousIo(thread);
        if (WaitForSingleObject(thread, cancel_wait_milliseconds) != WAIT_TIMEOUT)
        {
            return;
        }
    }
}
} // namespace

Win32Worker::~Win32Worker()
{
    // 起こしていなければ何もしない。壊すのは持ち主（UI スレッド）で、仕事はもう載らない。
    if (!started())
    {
        return;
    }
    // request_stop は condition_variable_any の待ちも起こす。
    thread_.request_stop();
    cancel_until_ended(thread_.native_handle());
    thread_.join();
}

void Win32Worker::post(Job job)
{
    {
        const std::scoped_lock lock(mutex_);
        jobs_.push_back(std::move(job));
        start();
    }
    wake_.notify_one();
}

// mutex_ を持って呼ぶ。最初の仕事で 1 度だけスレッドを起こす（合成のときには起こさない）。
void Win32Worker::start()
{
    if (!thread_.joinable())
    {
        thread_ = std::jthread([this](const std::stop_token &stop) { run(stop); });
    }
}

bool Win32Worker::started() const
{
    const std::scoped_lock lock(mutex_);
    return thread_.joinable();
}

void Win32Worker::run(const std::stop_token &stop) noexcept
{
    // 例外はプログラムの欠陥として扱い、スレッドの外へは出さない（noexcept で止める・CPP-005）。
    for (auto job = next(stop); job.has_value(); job = next(stop))
    {
        job.value()(stop);
    }
}

std::optional<Win32Worker::Job> Win32Worker::next(const std::stop_token &stop)
{
    std::unique_lock lock(mutex_);
    static_cast<void>(wake_.wait(lock, stop, [this] { return !jobs_.empty(); }));
    if (stop.stop_requested() || jobs_.empty())
    {
        return std::nullopt;
    }
    auto job = std::move(jobs_.front());
    jobs_.pop_front();
    return job;
}
} // namespace nenenib::adapters::win32
