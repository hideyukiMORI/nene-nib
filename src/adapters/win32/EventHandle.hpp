#pragma once

#include <windows.h>

namespace nenenib::adapters::win32
{
// CreateEventW が返す HANDLE の唯一の所有者（CPP-016）。作れなければ nullptr を持ち、閉じない。
class EventHandle final
{
  public:
    explicit EventHandle(HANDLE handle) noexcept : handle_(handle) {}

    ~EventHandle()
    {
        if (handle_ != nullptr)
        {
            CloseHandle(handle_);
        }
    }

    EventHandle(const EventHandle &) = delete;
    EventHandle(EventHandle &&) = delete;
    EventHandle &operator=(const EventHandle &) = delete;
    EventHandle &operator=(EventHandle &&) = delete;

    [[nodiscard]] bool valid() const noexcept
    {
        return handle_ != nullptr;
    }

    [[nodiscard]] HANDLE get() const noexcept
    {
        return handle_;
    }

  private:
    HANDLE handle_;
};
} // namespace nenenib::adapters::win32
