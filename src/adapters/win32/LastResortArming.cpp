#include "LastResortArming.hpp"

#include <windows.h>

#include <bit>
#include <cstdlib>
#include <exception>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
// 手続きが読む値は process に 1 つ。UI スレッドの番号はワーカーの手続きも読むので、Win32 の
// 不可分な操作で読み書きする（`<atomic>` は CNF-009 で使わない）。hooks を読むのは UI スレッドの
// 手続きだけ。
[[nodiscard]] LastResortHooks &armed_hooks() noexcept
{
    static LastResortHooks hooks;
    return hooks;
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

[[noreturn]] void end_process() noexcept
{
    static_cast<void>(TerminateProcess(GetCurrentProcess(), last_resort_exit_code));
    // TerminateProcess は自分の process では戻らない。戻ったときだけ既定の終わり方に任せる。
    std::abort();
}

// 窓の手続きを OS の既定に替え、入力を止める（箱の modal の loop が壊れた状態の手続きを回さない）。
// 替えてから止める（EnableWindow は WM_ENABLE を同期で送る）。
BOOL CALLBACK quiet_window(HWND window, LPARAM) noexcept
{
    static_cast<void>(
        SetWindowLongPtrW(window, GWLP_WNDPROC, std::bit_cast<LONG_PTR>(&DefWindowProcW)));
    static_cast<void>(EnableWindow(window, FALSE));
    return TRUE;
}

BOOL CALLBACK quiet_top_window(HWND window, LPARAM) noexcept
{
    static_cast<void>(quiet_window(window, 0));
    static_cast<void>(EnumChildWindows(window, &quiet_window, 0));
    return TRUE;
}

[[nodiscard]] bool backed_up_now() noexcept
{
    try
    {
        const auto &backup = armed_hooks().backup;
        return backup && backup();
    }
    catch (...)
    {
        return false;
    }
}

// 知らせられたら真（知らせの口が無い・投げたら偽）。どちらでも終わり方は同じ。
[[nodiscard]] bool notified(bool backed_up) noexcept
{
    try
    {
        const auto &notice = armed_hooks().notice;
        if (!notice)
        {
            return false;
        }
        notice(backed_up);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

// std::terminate の手続き（1 つ）。UI スレッドのときだけ退避と知らせを通す。
void last_resort() noexcept
{
    if (InterlockedExchange(&entered(), 1) != 0)
    {
        end_process();
    }
    const auto thread = static_cast<DWORD>(InterlockedCompareExchange(&armed_thread(), 0, 0));
    if (thread == 0 || thread != GetCurrentThreadId() || !armed_hooks().backup)
    {
        end_process();
    }
    const bool backed_up = backed_up_now();
    static_cast<void>(EnumThreadWindows(GetCurrentThreadId(), &quiet_top_window, 0));
    static_cast<void>(notified(backed_up));
    end_process();
}
} // namespace

LastResortArming::LastResortArming(LastResortHooks hooks)
{
    armed_hooks() = std::move(hooks);
    static_cast<void>(
        InterlockedExchange(&armed_thread(), static_cast<LONG>(GetCurrentThreadId())));
    static_cast<void>(std::set_terminate(&last_resort));
}

// 手続きは残し、UI スレッドの印と hooks だけを外す（以後の terminate は退避せずに終える）。
LastResortArming::~LastResortArming()
{
    static_cast<void>(InterlockedExchange(&armed_thread(), 0));
    armed_hooks() = LastResortHooks{};
}

void arm_last_resort_thread() noexcept
{
    static_cast<void>(std::set_terminate(&last_resort));
}
} // namespace nenenib::adapters::win32
