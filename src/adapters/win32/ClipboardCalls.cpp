#include "ClipboardCalls.hpp"

namespace nenenib::adapters::win32
{
ClipboardCalls system_clipboard_calls() noexcept
{
    return ClipboardCalls{
        .open = &OpenClipboard,
        .close = &CloseClipboard,
        .empty = &EmptyClipboard,
        .set = &SetClipboardData,
        .get = &GetClipboardData,
        .available = &IsClipboardFormatAvailable,
        .wait = &Sleep,
    };
}
} // namespace nenenib::adapters::win32
