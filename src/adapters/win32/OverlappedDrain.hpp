#pragma once

#include <windows.h>

namespace nenenib::adapters::win32
{
// 出ている OVERLAPPED の I/O に取消を頼み、最終完了を待ってから戻る（ADR 0126 の決定 6・9）。
// 取消の返値は観測だけで、終わりの証明は GetOverlappedResult の待ちで得る。待ちが未完を返すのは
// handle と OVERLAPPED の不変条件の破れなので、資源を離さずにその場で落とす（fail-fast）。
void drain_overlapped(HANDLE handle, OVERLAPPED &overlapped) noexcept;
} // namespace nenenib::adapters::win32
