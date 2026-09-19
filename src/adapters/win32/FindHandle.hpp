#pragma once

#include <windows.h>

namespace nenenib::adapters::win32
{
// FindFirstFileW の検索ハンドルは CloseHandle ではなく FindClose で閉じる。
class FindHandle final
{
  public:
    explicit FindHandle(HANDLE handle) noexcept : handle_(handle) {}
    ~FindHandle()
    {
        if (valid())
        {
            FindClose(handle_);
        }
    }
    FindHandle(const FindHandle &) = delete;
    FindHandle(FindHandle &&) = delete;
    FindHandle &operator=(const FindHandle &) = delete;
    FindHandle &operator=(FindHandle &&) = delete;
    [[nodiscard]] bool valid() const noexcept
    {
        return handle_ != INVALID_HANDLE_VALUE;
    }
    [[nodiscard]] HANDLE get() const noexcept
    {
        return handle_;
    }

  private:
    HANDLE handle_;
};
} // namespace nenenib::adapters::win32
