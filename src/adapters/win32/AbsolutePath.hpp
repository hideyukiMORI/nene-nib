#pragma once

#include "FilePath.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
// 起動引数の相対経路を絶対経路の core::FilePath にする（ADR 0010 の決定 11 / CPP-014）。
// 現在のフォルダを読むので OS に触れる区画に置く。合成ルートから 1 度だけ呼ぶ。
[[nodiscard]] std::optional<core::FilePath> absolute_file_path(const std::wstring &path);
// ドライブの絶対（`C:\` `C:/`）か UNC（`\\`）で始まるか。設定の置き場の検査と前回のタブの一覧の
// 経路の検査が同じ規則を使う（ADR 0020 / ADR 0059 の決定 2）。文字列だけを見て OS に問わない。
[[nodiscard]] bool rooted_path_text(std::string_view text) noexcept;
} // namespace nenenib::adapters::win32
