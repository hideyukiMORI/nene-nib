#pragma once

#include <array>
#include <string_view>

namespace nenenib::core
{
// 同じフォルダの一覧に出さない拡張子の表（ADR 0062 の決定 12・施主決定 D33）。よく知られた
// 「テキストでない」拡張子だけを小文字の ASCII で 1 回ずつ書く。並びは画像・実行と中間・圧縮・
// 文書・音と動画・フォント・データベースの順。開いているタブと履歴には当てない。
inline constexpr auto unlisted_extensions = std::to_array<std::string_view>(
    {"png",   "jpg", "jpeg", "gif", "bmp",  "ico",   "webp", "tif",    "tiff", "heic", "psd", "exe",
     "dll",   "sys", "com",  "msi", "scr",  "lib",   "obj",  "pdb",    "o",    "a",    "so",  "bin",
     "class", "pyc", "lnk",  "zip", "7z",   "rar",   "gz",   "tgz",    "tar",  "bz2",  "xz",  "cab",
     "iso",   "lzh", "pdf",  "doc", "docx", "xls",   "xlsx", "ppt",    "pptx", "odt",  "ods", "odp",
     "mp3",   "wav", "flac", "ogg", "m4a",  "aac",   "wma",  "mp4",    "mov",  "avi",  "mkv", "wmv",
     "webm",  "ttf", "otf",  "ttc", "woff", "woff2", "db",   "sqlite", "mdb",  "accdb"});

// 同じフォルダの一覧に name（ファイルの名前）を出すか。名前の最後の '.' の後ろを、ASCII の大文字と
// 小文字を区別せずに表で引き、表にあれば出さない。'.' の無い名前・'.' で終わる名前・表に無い
// 拡張子は出す。'.' が先頭の 1 つだけの名前（`.gitignore` `.png` `.a`）は拡張子なしとして出し、
// 後ろにもう 1 つ '.' があれば（`.config.png`）最後の '.' の後ろで引く。OS とロケールに触れない
// （ARC-003）。
[[nodiscard]] bool folder_lists(std::string_view name) noexcept;
} // namespace nenenib::core
