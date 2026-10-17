#pragma once

#include <windows.h>

namespace nenenib::adapters::win32
{
// Win32ClipboardAdapter が呼ぶ OS の関数の束（ADR 0134 の決定 4）。製品は system_clipboard_calls()
// の本物を使い、試験は替え玉の関数を詰めて「開けない」「置けない」を作る。実機のクリップボードを
// 試験で触らないための差し替え口で、ほかの用途に使わない。
struct ClipboardCalls
{
    decltype(&OpenClipboard) open;
    decltype(&CloseClipboard) close;
    decltype(&EmptyClipboard) empty;
    decltype(&SetClipboardData) set;
    decltype(&GetClipboardData) get;
    decltype(&IsClipboardFormatAvailable) available;
    decltype(&Sleep) wait;
};

[[nodiscard]] ClipboardCalls system_clipboard_calls() noexcept;
} // namespace nenenib::adapters::win32
