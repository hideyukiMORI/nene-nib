#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

namespace nenenib::adapters::win32
{
// 裏の仕事を走らせる固定のワーカー 1 本（ADR 0004・ADR 0062 の決定 5 / 9・CPP-013）。
// スレッド 1 本と仕事のキュー 1 本を持ち、仕事は載せた順に 1 つずつ走る。
// スレッドは最初の仕事を受けたときに起こす。application からは見えない。
// 仕事は止める合図を受け取り、I/O の呼び出しの間でそれを見て戻る。
// 壊すときは、止める合図を出して待っているワーカーを起こし、止まっている同期の I/O を
// CancelSynchronousIo で起こしてスレッドの終わりを待つ（TerminateThread は使わない）。
class Win32Worker final
{
  public:
    using Job = std::move_only_function<void(const std::stop_token &)>;

    Win32Worker() = default;
    ~Win32Worker();
    Win32Worker(const Win32Worker &) = delete;
    Win32Worker(Win32Worker &&) = delete;
    Win32Worker &operator=(const Win32Worker &) = delete;
    Win32Worker &operator=(Win32Worker &&) = delete;

    // 仕事を載せてすぐ戻る。UI スレッドと、仕事の中（合図の中）から呼んでよい。
    void post(Job job);
    // スレッドを起こしたか（試験が「1 度も載せなければ起こさない」を見る口）。
    [[nodiscard]] bool started() const;

  private:
    void start();
    void run(const std::stop_token &stop) noexcept;
    [[nodiscard]] std::optional<Job> next(const std::stop_token &stop);

    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::deque<Job> jobs_;
    std::jthread thread_;
};
} // namespace nenenib::adapters::win32
