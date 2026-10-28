#pragma once

#include <windows.h>

namespace nenenib::adapters::win32
{
// 言語サーバーの子プロセス・Job Object・パイプの HANDLE の唯一の所有者（CPP-016・ADR 0160）。
// nullptr と INVALID_HANDLE_VALUE はどちらも「持っていない」として閉じない。
// 寿命の途中で閉じる順を決める所（パイプを閉じてから子の終了を待つ）のために close を持つ。
class KernelHandle final
{
  public:
    KernelHandle() noexcept = default;
    explicit KernelHandle(HANDLE handle) noexcept : handle_(handle) {}

    ~KernelHandle()
    {
        close();
    }

    KernelHandle(const KernelHandle &) = delete;
    KernelHandle(KernelHandle &&) = delete;
    KernelHandle &operator=(const KernelHandle &) = delete;
    KernelHandle &operator=(KernelHandle &&) = delete;

    [[nodiscard]] bool valid() const noexcept
    {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }

    [[nodiscard]] HANDLE get() const noexcept
    {
        return handle_;
    }

    // 持っていたものを閉じて、新しい handle を持つ。
    void reset(HANDLE handle) noexcept
    {
        close();
        handle_ = handle;
    }

    void close() noexcept
    {
        if (valid())
        {
            CloseHandle(handle_);
        }
        handle_ = nullptr;
    }

  private:
    HANDLE handle_ = nullptr;
};
} // namespace nenenib::adapters::win32
