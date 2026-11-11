#pragma once

#include "FilePath.hpp"
#include "ImageFailure.hpp"

#include <expected>
#include <string>

namespace nenenib::adapters::win32
{
// 画像のファイルを丸ごと読む（施主決定 D79・ADR 0155 の追補 54・#1412）。経路は core が解いたローカ
// ルの経路で、native_path を通して開く。決まり:
// - 無い・フォルダ → missing。開けない・読めない（拒否・共有違反・読みの失敗）→ unreadable。
// - 開いた後に GetFileType が FILE_TYPE_DISK でなければ unreadable（予約のデバイス名 `NUL` `CON`
//   `COM1` `CONIN$` は core が経路として通すので、名前でなく OS の判定で断る）。
// - 大きさが core::image_file_byte_limit を超えれば読まずに too_large。
// 読む途中で記憶が足りなければ std::bad_alloc が出る（呼ぶ側が insufficient_memory にする）。
[[nodiscard]] std::expected<std::string, application::ImageFailure>
image_file_bytes(const core::FilePath &path);
} // namespace nenenib::adapters::win32
