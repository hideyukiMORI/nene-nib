#pragma once

#include "FilePath.hpp"

#include <optional>

namespace nenenib::core
{
// path のファイルがあるフォルダ（ADR 0062 の決定 13）。同じフォルダを列挙するときに渡す値で、最後の
// 区切り（'\' か '/'）の前まで。ルート直下は区切りを残す（`C:\a.txt` → `C:\`・`\a.txt` → `\`）。
// UNC は `\\server\share\a.txt` → `\\server\share`。区切りで終わる経路は最後の区切りの前
// （`C:\work\` → `C:\work`・`C:\` → `C:\`）。区切りの無い経路（名前だけ・`C:a.txt`）は無し。
// 表示用の場所は tab_folder_for で、こちらとは別の値（DisplayText と FilePath）。
[[nodiscard]] std::optional<FilePath> folder_of(const FilePath &path);

// path が folder そのものか、その下にあるか（ADR 0161 の 2026-11-22 の追補・段 S3 の設定の root）。
// 文字を比べるだけでファイルに触れない。ASCII の大小と区切りの `\` `/` を区別せず、folder の
// 後ろは区切りの境目でなければならない（`C:\work` は `C:\workshop\a.cpp` を含まない）。
[[nodiscard]] bool folder_contains(const FilePath &folder, const FilePath &path) noexcept;
} // namespace nenenib::core
