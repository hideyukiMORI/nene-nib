#pragma once

#include "FilePath.hpp"

#include <string>

namespace nenenib::application
{
// ファイルのパス → 言語サーバーへ渡す `file:` の URI（ADR 0161 の決定 1）。`C:\a b\x.cpp` は
// `file:///C:/a%20b/x.cpp`、UNC の `\host\share\x` は `file://host/share/x`。区切りの `\` は `/`
// に、英数字と `- . _ ~ / :` のほかのバイト（UTF-8 の各バイトを含む）は `%XX`（大文字の 16 進）に
// する。OS に触れない純関数で、届いた診断の文書は送った URI の文字列の一致で引く。
[[nodiscard]] std::string document_uri(const core::FilePath &path);
} // namespace nenenib::application
