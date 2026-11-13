#include "LastResortArming.hpp"

#include <windows.h>

#include <cstdlib>
#include <exception>
#include <functional>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
// 手続きが読む値は process に 1 つ。UI スレッドの番号はワーカーの手続きも読むので、Win32 の
// 不可分な操作で読み書きする（`<atomic>` は CNF-009 で使わない）。backup を読むのは UI スレッドの
// 手続きだけ。
[[nodiscard]] std::function<bool()> &armed_backup() noexcept
{
    static std::function<bool()> backup;
    return backup;
}

[[nodiscard]] LONG &armed_thread() noexcept
{
    static LONG thread = 0;
    return thread;
}

[[nodiscard]] LONG &entered() noexcept
{
    static LONG value = 0;
    return value;
}

// 呼んだスレッドで set_terminate が返した前の手続き（UCRT はスレッドごとの欄で、既定は abort）。
[[nodiscard]] std::terminate_handler &previous_handler() noexcept
{
    thread_local std::terminate_handler previous = nullptr;
    return previous;
}

// 既定の終わり方へ渡す（前の手続き・無ければ abort）。前の手続きが戻れば abort で終える。
[[noreturn]] void end_by_default() noexcept
{
    const std::terminate_handler previous = previous_handler();
    if (previous != nullptr)
    {
        previous();
    }
    std::abort();
}

// 退避を 1 回だけ回す。投げたら退避できなかったと読み、終わり方は同じ（次の起動は前回の定期の
// 退避を拾う）。
void back_up_once() noexcept
{
    try
    {
        const auto &backup = armed_backup();
        if (backup)
        {
            static_cast<void>(backup());
        }
    }
    catch (...)
    {
        return;
    }
}

// std::terminate の手続き（1 つ）。UI スレッドのときだけ退避を通し、どのスレッドでも既定へ渡す。
void last_resort() noexcept
{
    if (InterlockedExchange(&entered(), 1) == 0)
    {
        const auto thread = static_cast<DWORD>(InterlockedCompareExchange(&armed_thread(), 0, 0));
        if (thread != 0 && thread == GetCurrentThreadId())
        {
            back_up_once();
        }
    }
    end_by_default();
}

// 呼んだスレッドに手続きを置き、前の手続きを覚える（同じスレッドで 2 回置いても自分を覚えない）。
void arm_this_thread() noexcept
{
    const std::terminate_handler previous = std::set_terminate(&last_resort);
    if (previous != &last_resort)
    {
        previous_handler() = previous;
    }
}
} // namespace

LastResortArming::LastResortArming(std::function<bool()> backup)
{
    armed_backup() = std::move(backup);
    static_cast<void>(
        InterlockedExchange(&armed_thread(), static_cast<LONG>(GetCurrentThreadId())));
    arm_this_thread();
}

// UI スレッドの印と backup を外し、UI スレッドの手続きを前のものへ戻す（壊すのは作ったスレッド）。
LastResortArming::~LastResortArming()
{
    static_cast<void>(InterlockedExchange(&armed_thread(), 0));
    armed_backup() = std::function<bool()>{};
    const std::terminate_handler previous = previous_handler();
    if (previous != nullptr)
    {
        static_cast<void>(std::set_terminate(previous));
    }
}

void arm_last_resort_thread() noexcept
{
    arm_this_thread();
}
} // namespace nenenib::adapters::win32
