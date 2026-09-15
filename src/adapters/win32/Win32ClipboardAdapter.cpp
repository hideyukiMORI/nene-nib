#include "Win32ClipboardAdapter.hpp"

#include "Utf16.hpp"

#include <cstring>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::ClipboardFailure;

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
    // 変換できない本文は空になる。利用者の既存のクリップボードは下で置き換えるまで残る。
    const std::wstring wide = core::to_utf16(utf8).value_or(std::wstring{});
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
    std::string utf8 = core::to_utf8(text_of(memory)).value_or(std::string{});
    GlobalUnlock(handle);
    CloseClipboard();
    return utf8;
}
} // namespace nenenib::adapters::win32
