#pragma once

#include "PrintFailure.hpp"
#include "PrintSettings.hpp"

#include <expected>
#include <optional>
#include <windows.h>

namespace nenenib::ui::win32
{
// OS の印刷のダイアログ（PrintDlgExW・ADR 0177 の決定 3・10）。ui の操作「印刷」が意図を送る前に
// 呼ぶ（controller の apply の中で modal loop を回さない）。owner は本窓。「印刷」を押せば紙と範囲
// とプリンタの名前と print ticket、取り消せば nullopt。プリンタが 1 台も無ければ no_printer、
// ダイアログが開けなければ dialog_failed。comdlg32 と prntvpt は遅延読み込み（決定 6）なので、
// DLL はここを最初に呼んだときに読む。
[[nodiscard]] std::expected<std::optional<application::PrintSettings>, application::PrintFailure>
choose_printer(HWND owner);
} // namespace nenenib::ui::win32
