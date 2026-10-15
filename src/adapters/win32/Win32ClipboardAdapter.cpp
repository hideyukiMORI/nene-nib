#include "Win32ClipboardAdapter.hpp"

#include "Utf16.hpp"

#include <cstring>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::ClipboardFailure;

// ほかのプロセスが一瞬クリップボードを開いている間は待って開き直す（ADR 0134 の決定 2）。
// 最初の 1 回の後に 10 ms ずつ 5 回まで試し、合わせて 50 ms で諦める。回数と間隔はここだけ。
constexpr int open_retries = 5;
constexpr DWORD open_retry_milliseconds = 10;

// CF_UNICODETEXT に置く NUL 終端の複写を作る。作れなければ nullptr。
[[nodiscard]] HGLOBAL global_text(const std::wstring &wide) noexcept
{
    const std::size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (handle == nullptr)
    {
        return nullptr;
    }
    void *memory = GlobalLock(handle);
    if (memory == nullptr)
    {
        GlobalFree(handle);
        return nullptr;
    }
    std::memcpy(memory, wide.c_str(), bytes);
    GlobalUnlock(handle);
    return handle;
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

Win32ClipboardAdapter::Win32ClipboardAdapter() noexcept
    : Win32ClipboardAdapter(system_clipboard_calls())
{
}

Win32ClipboardAdapter::Win32ClipboardAdapter(ClipboardCalls calls) noexcept : calls_(calls) {}

void Win32ClipboardAdapter::bind(HWND owner) noexcept
{
    owner_ = owner;
}

// 書きと読みが通る唯一の開く口（ADR 0134 の決定 2）。開けたら呼び手が閉じる。
std::expected<void, Failure> Win32ClipboardAdapter::opened_clipboard()
{
    if (owner_ == nullptr)
    {
        return std::unexpected(Failure::unavailable);
    }
    for (int retry = 0; calls_.open(owner_) == 0; ++retry)
    {
        if (retry == open_retries)
        {
            return std::unexpected(Failure::unavailable);
        }
        calls_.wait(open_retry_milliseconds);
    }
    return {};
}

std::expected<void, Failure> Win32ClipboardAdapter::store(HGLOBAL handle)
{
    if (calls_.empty() == 0)
    {
        return std::unexpected(Failure::write_failed);
    }
    if (calls_.set(CF_UNICODETEXT, handle) == nullptr)
    {
        return std::unexpected(Failure::write_failed);
    }
    return {};
}

std::expected<void, Failure> Win32ClipboardAdapter::write(std::string_view utf8)
{
    // 変換できない本文は書かない。空文字列を置いて成功と言わない（ADR 0134 の決定 1）。
    const auto wide = core::to_utf16(utf8);
    if (!wide)
    {
        return std::unexpected(Failure::unrepresentable);
    }
    // 確保と複写を開く前に済ませる。ここで失敗しても利用者の既存のクリップボードは消さない。
    HGLOBAL handle = global_text(wide.value());
    if (handle == nullptr)
    {
        return std::unexpected(Failure::write_failed);
    }
    const auto opened = opened_clipboard();
    if (!opened)
    {
        GlobalFree(handle);
        return std::unexpected(opened.error());
    }
    // 空にした後で置けなければ前の中身は戻らない。write_failed を返し、呼び手が知らせる（決定 3）。
    const auto stored = store(handle);
    calls_.close();
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
    if (calls_.available(CF_UNICODETEXT) == 0)
    {
        return std::unexpected(Failure::unsupported_format);
    }
    const auto opened = opened_clipboard();
    if (!opened)
    {
        return std::unexpected(opened.error());
    }
    HANDLE handle = calls_.get(CF_UNICODETEXT);
    if (handle == nullptr)
    {
        calls_.close();
        return std::unexpected(Failure::empty);
    }
    const auto *memory = static_cast<const wchar_t *>(GlobalLock(handle));
    if (memory == nullptr)
    {
        calls_.close();
        return std::unexpected(Failure::read_failed);
    }
    std::string utf8 = core::to_utf8(text_of(memory)).value_or(std::string{});
    GlobalUnlock(handle);
    calls_.close();
    return utf8;
}
} // namespace nenenib::adapters::win32
