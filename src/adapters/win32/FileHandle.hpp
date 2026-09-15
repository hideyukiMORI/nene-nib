#pragma once

#include <windows.h>

namespace nenenib::adapters::win32
{
// CreateFileW が返す HANDLE の唯一の所有者（CPP-016）。閉じるのはここだけで、複製はできない。
class FileHandle final
{
  public:
    explicit FileHandle(HANDLE handle) noexcept : handle_(handle) {}

    ~FileHandle()
    {
        if (handle_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle_);
        }
    }

    FileHandle(const FileHandle &) = delete;
    FileHandle(FileHandle &&) = delete;
    FileHandle &operator=(const FileHandle &) = delete;
    FileHandle &operator=(FileHandle &&) = delete;

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
