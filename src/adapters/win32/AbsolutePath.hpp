#pragma once

#include "FilePath.hpp"

#include <optional>
#include <string>

namespace nenenib::adapters::win32
{
// 起動引数の相対経路を絶対経路の core::FilePath にする（ADR 0010 の決定 11 / CPP-014）。
// 現在のフォルダを読むので OS に触れる区画に置く。合成ルートから 1 度だけ呼ぶ。
[[nodiscard]] std::optional<core::FilePath> absolute_file_path(const std::wstring &path);
} // namespace nenenib::adapters::win32
