#pragma once

#include <string>
#include <string_view>

namespace nenenib::core
{
// OS のクリップボードの本文の改行を LF に畳む（ADR 0055 の決定 1・ADR 0051 の決定 4）。この 1 本
// だけが畳む（ARC-001）。`\r\n` を `\n` に畳み、単独の `\r` は文字として残す（`\r\r\n` は `\r` と
// 改行）。Ctrl+V の貼り付けと `"+` `"*` の写しが同じ規則で読む。文字列の純関数で OS に触れない
// （ARC-007）。
[[nodiscard]] std::string clipboard_line_feeds(std::string_view text);
} // namespace nenenib::core
