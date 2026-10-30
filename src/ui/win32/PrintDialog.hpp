#pragma once

#include "PrintChoice.hpp"
#include "PrintFailure.hpp"

#include <expected>
#include <optional>
#include <windows.h>

namespace nenenib::ui::win32
{
// OS の印刷のダイアログ（PrintDlgExW・ADR 0177 の決定 3）。owner はダイアログの親の窓。「印刷」を
// 押せば紙と範囲とプリンタの名前と print ticket、取り消せば nullopt。プリンタが 1 台も無ければ
// no_printer、ダイアログが開けなければ dialog_failed。comdlg32 と prntvpt は遅延読み込み（決定 6）
// なので、DLL はここを最初に呼んだときに読む。
[[nodiscard]] std::expected<std::optional<PrintChoice>, application::PrintFailure>
choose_printer(HWND owner);
} // namespace nenenib::ui::win32
