#pragma once

#include "FilePath.hpp"

#include <optional>
#include <string_view>

namespace nenenib::core
{
// 言語サーバーの答えの URI → ファイルのパス（ADR 0190 の決定 4・application の document_uri の
// 逆）。`file:` の URI だけを受ける（scheme の大小は問わない）。`file:///C:/a%20b/c.cpp` は
// `C:\a b\c.cpp`、host の無い `file:/C:/x` と host が `localhost` の形も同じ。host のある
// `file://server/share/x` は UNC の `\\server\share\x`。写しは core の file_url_path の 1 本
// （FileUrlReading::language_server）で、`?` と `#` の後ろを捨て、percent-encoding を解き、区切りを
// `\` にし、`.` と `..` の成分を字句で畳み、ドライブ文字を大文字にそろえる。`file:` 以外の scheme・
// ドライブの後ろに区切りの無いパス（`C:foo`・`C:`）・ドライブの無い host なしのパス・device の
// 名前空間（`file://./x` `file://%3F/x`）・share の無い UNC・畳めない `..`・正しい UTF-8 でないか
// 制御文字（NUL を含む）を含むパスは値なし。OS に触れない純関数。
[[nodiscard]] std::optional<FilePath> file_path_of_uri(std::string_view uri);
} // namespace nenenib::core
