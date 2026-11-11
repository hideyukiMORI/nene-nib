#pragma once

#include "FilePath.hpp"

#include <windows.h>

#include <optional>
#include <span>
#include <string>

namespace nenenib::ui::win32
{
// ファイルを選ぶ窓（ADR 0010 の決定 10）。COM のダイアログは窓の一部で、ファイルには触れない。
// 取り消しは失敗ではないので std::optional で返す（CPP-004 / CPP-005）。
[[nodiscard]] std::optional<core::FilePath> choose_file_to_open(HWND owner);
[[nodiscard]] std::optional<core::FilePath> choose_file_to_save(HWND owner,
                                                                const std::wstring &suggested);
// エクスプローラーのルートのフォルダを選ぶ窓（ADR 0162 の決定
// 7・FOS_PICKFOLDERS）。取り消しは値なし。places（UTF-8 の経路・WSL のディストリなど）は箱の
// 左の一覧の下に足す。作れない・足せない場所は黙って外す（ADR 0162 の追補 #1450）。
[[nodiscard]] std::optional<core::FilePath> choose_folder(HWND owner,
                                                          std::span<const std::string> places);
} // namespace nenenib::ui::win32
