#include "OverlappedDrain.hpp"

namespace nenenib::adapters::win32
{
void drain_overlapped(HANDLE handle, OVERLAPPED &overlapped) noexcept
{
    CancelIoEx(handle, &overlapped);
    DWORD transferred = 0;
    if (GetOverlappedResult(handle, &overlapped, &transferred, TRUE) == 0 &&
        GetLastError() == ERROR_IO_INCOMPLETE)
    {
        RaiseFailFastException(nullptr, nullptr, 0);
    }
}
} // namespace nenenib::adapters::win32
