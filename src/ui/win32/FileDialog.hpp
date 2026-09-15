#pragma once

#include "FilePath.hpp"

#include <windows.h>

#include <optional>
#include <string>

namespace nenenib::ui::win32
{
// ファイルを選ぶ窓（ADR 0010 の決定 10）。COM のダイアログは窓の一部で、ファイルには触れない。
// 取り消しは失敗ではないので std::optional で返す（CPP-004 / CPP-005）。
[[nodiscard]] std::optional<core::FilePath> choose_file_to_open(HWND owner);
[[nodiscard]] std::optional<core::FilePath> choose_file_to_save(HWND owner,
                                                                const std::wstring &suggested);
} // namespace nenenib::ui::win32
