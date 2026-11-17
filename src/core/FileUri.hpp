#pragma once

#include "FilePath.hpp"

#include <optional>
#include <string_view>

namespace nenenib::core
{
// 言語サーバーの答えの URI → ファイルのパス（ADR 0190 の決定 4・application の document_uri の
// 逆）。`file:` の URI だけを受ける（scheme の大小は問わない）。`file:///C:/a%20b/c.cpp` は
// `C:\a b\c.cpp`、host の無い `file:/C:/x` と host が `localhost` の形も同じ。host のある
// `file://server/share/x` は UNC の `\\server\share\x`。percent-encoding は percent_decoded の
// 1 本で解き、区切りの `/` は `\` にする。`file:` 以外の scheme・ドライブの無い host なしの
// パス・正しい UTF-8 でないか制御文字を含むパスは値なし。OS に触れない純関数。
[[nodiscard]] std::optional<FilePath> file_path_of_uri(std::string_view uri);
} // namespace nenenib::core
