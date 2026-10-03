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
} // namespace nenenib::core
