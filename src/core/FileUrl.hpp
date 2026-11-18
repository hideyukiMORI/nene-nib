#pragma once

#include "CollapsedPath.hpp"
#include "FileUrlReading.hpp"
#include "FileUrlRefusal.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::core
{
// `file:` の URL → Windows のパスの唯一の写し（ARC-012・#1825 のレビュー R2）。画像の参照
// （ImageReference）と言語サーバーの URI（file_path_of_uri）が使う。OS に触れない純関数。

// `/` か `\`。
[[nodiscard]] bool path_separator(char value) noexcept;

// `?` か `#` より前（URL の query と fragment を捨てる）。
[[nodiscard]] std::string_view url_without_suffix(std::string_view text) noexcept;

// `C:\` か `C:/` で始まる（ドライブの後ろの区切りは必須・`C:a` はドライブの相対パス）。
[[nodiscard]] bool drive_absolute_path(std::string_view text) noexcept;

// root（`C:` か `\\host\share`）の後ろの rest を `/` か `\` で成分に分け、空と `.` を落として `..`
// を 1 つ上へ畳み、root + `\` + 成分を `\` でつないだパスにする。根より上へ出る `..` は捨てて
// above_root を立てる。
[[nodiscard]] CollapsedPath collapsed_path(std::string_view root, std::string_view rest);

// `file:` の後ろ rest → パス（呼び出し元の規則は FileUrlReading）。`?` と `#`
// の後ろを捨て、`//host` を分け、percent-encoding を percent_decoded の 1 本で解き、host が空か
// localhost なら `/C:/x` の頭の
// `/` を落としたドライブの絶対パスを、host があれば（language_server だけ）`\\host\share\x` を、
// collapsed_path で畳んで返す。答えが正しい UTF-8 か・制御文字を含まないかは FilePath::parse で
// 呼び出し元が確かめる。
[[nodiscard]] std::expected<std::string, FileUrlRefusal> file_url_path(std::string_view rest,
                                                                       FileUrlReading reading);
} // namespace nenenib::core
