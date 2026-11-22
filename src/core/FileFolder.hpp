#pragma once

#include "FilePath.hpp"

#include <optional>
#include <string_view>

namespace nenenib::core
{
// path のファイルがあるフォルダ（ADR 0062 の決定 13）。同じフォルダを列挙するときに渡す値で、最後の
// 区切り（'\' か '/'）の前まで。ルート直下は区切りを残す（`C:\a.txt` → `C:\`・`\a.txt` → `\`）。
// UNC は `\\server\share\a.txt` → `\\server\share`。区切りで終わる経路は最後の区切りの前
// （`C:\work\` → `C:\work`・`C:\` → `C:\`）。区切りの無い経路（名前だけ・`C:a.txt`）は無し。
// 表示用の場所は tab_folder_for で、こちらとは別の値（DisplayText と FilePath）。
[[nodiscard]] std::optional<FilePath> folder_of(const FilePath &path);

// フォルダの経路の文字から末尾の区切り 1 つを除いた部分（`C:\work\` → `C:\work`）。根の `C:\`
// と `\` は根のまま。folder_contains・same_folder・上る上限（lsp_root_folders）が同じ畳み方を
// 使うための 1 本（#1963 のレビュー M1）。
[[nodiscard]] std::string_view folder_spelling(const FilePath &folder) noexcept;

// path が folder そのものか、その下にあるか（ADR 0161 の 2026-11-22 の追補・段 S3 の設定の root）。
// 文字を比べるだけでファイルに触れない。ASCII の大小と区切りの `\` `/` と folder の末尾の区切りを
// 区別せず（`C:\work\` は `C:\work` を含む）、folder の後ろは区切りの境目でなければならない
// （`C:\work` は `C:\workshop\a.cpp` を含まない）。
[[nodiscard]] bool folder_contains(const FilePath &folder, const FilePath &path) noexcept;

// left と right が同じフォルダか（folder_contains と同じ畳み方: ASCII の大小・`\` `/`・末尾の
// 区切り・#1963 のレビュー M1 / R1）。言語サーバーの root の席の鍵と覚えの鍵、設定の root の直下の
// 判定が使う。ファイルに触れない。
[[nodiscard]] bool same_folder(const FilePath &left, const FilePath &right) noexcept;
} // namespace nenenib::core
