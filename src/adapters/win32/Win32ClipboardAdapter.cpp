#include "Win32ClipboardAdapter.hpp"

#include <cstring>
#include <vector>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::ClipboardFailure;

[[nodiscard]] std::wstring widen(std::string_view utf8)
{
    const auto bytes = static_cast<int>(utf8.size());
    const int length =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), bytes, nullptr, 0);
    if (length <= 0)
    {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), bytes, wide.data(), length);
    return wide;
}

[[nodiscard]] std::string narrow(std::wstring_view wide)
{
    const auto units = static_cast<int>(wide.size());
    const int bytes =
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), units, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0)
    {
        return {};
    }
    std::string utf8(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), units, utf8.data(), bytes, nullptr, nullptr);
    return utf8;
}

// CF_UNICODETEXT は NUL で終わる。長さは終端までを数える。
[[nodiscard]] std::wstring_view text_of(const wchar_t *memory) noexcept
{
    std::size_t length = 0;
    while (memory[length] != L'\0')
    {
        ++length;
    }
    return std::wstring_view(memory, length);
}
} // namespace

void Win32ClipboardAdapter::bind(HWND owner) noexcept
{
    owner_ = owner;
}

std::expected<void, Failure> Win32ClipboardAdapter::store(HGLOBAL handle)
{
    if (EmptyClipboard() == 0)
    {
        return std::unexpected(Failure::write_failed);
    }
    if (SetClipboardData(CF_UNICODETEXT, handle) == nullptr)
    {
        return std::unexpected(Failure::write_failed);
    }
    return {};
}

std::expected<void, Failure> Win32ClipboardAdapter::write(std::string_view utf8)
{
    if (owner_ == nullptr)
    {
        return std::unexpected(Failure::unavailable);
    }
    const std::wstring wide = widen(utf8);
    // 確保と複写を先に済ませる。ここで失敗しても利用者の既存のクリップボードは消さない。
    const std::size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (handle == nullptr)
    {
        return std::unexpected(Failure::write_failed);
    }
    void *memory = GlobalLock(handle);
    if (memory == nullptr)
    {
        GlobalFree(handle);
        return std::unexpected(Failure::write_failed);
    }
    std::memcpy(memory, wide.c_str(), bytes);
    GlobalUnlock(handle);
    if (OpenClipboard(owner_) == 0)
    {
        GlobalFree(handle);
        return std::unexpected(Failure::unavailable);
    }
    const auto stored = store(handle);
    CloseClipboard();
    // 置けたときだけ所有権がシステムへ移る。置けなければこちらで解放する。
    if (!stored)
    {
        GlobalFree(handle);
    }
    return stored;
}

std::expected<std::string, Failure> Win32ClipboardAdapter::read()
{
    if (owner_ == nullptr)
    {
        return std::unexpected(Failure::unavailable);
    }
    if (IsClipboardFormatAvailable(CF_UNICODETEXT) == 0)
    {
        return std::unexpected(Failure::unsupported_format);
    }
    if (OpenClipboard(owner_) == 0)
    {
        return std::unexpected(Failure::unavailable);
    }
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (handle == nullptr)
    {
        CloseClipboard();
        return std::unexpected(Failure::empty);
    }
    const auto *memory = static_cast<const wchar_t *>(GlobalLock(handle));
    if (memory == nullptr)
    {
        CloseClipboard();
        return std::unexpected(Failure::read_failed);
    }
    std::string utf8 = narrow(text_of(memory));
    GlobalUnlock(handle);
    CloseClipboard();
    return utf8;
}
} // namespace nenenib::adapters::win32
