#pragma once

#include "PrintPaper.hpp"

#include <windows.h>

namespace nenenib::ui::win32
{
// 印刷のダイアログの DEVMODE から紙の大きさ（DIP・向きは入れ替え済み）を引く（ADR 0177 の決定 5）。
// 幅と長さ（0.1 mm）があればそれ、無ければ用紙の番号の表（A3・A4・A5・B4・B5・Letter・Legal・
// Tabloid・Executive）、どちらも無ければ A4。GDI の DC を作らない（ui は GDI
// を結ばない・CPP-017）。
[[nodiscard]] application::PrintPaper print_paper(const DEVMODEW &mode) noexcept;
} // namespace nenenib::ui::win32
