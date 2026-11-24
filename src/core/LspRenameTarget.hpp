#pragma once

#include "LspRange.hpp"

#include <optional>
#include <string>

namespace nenenib::core
{
// `textDocument/prepareRename` の答えの名前を変えられる所（ADR 0194 の決定 7）。`Range` は range
// だけ、`{range, placeholder}` は両方、`{defaultBehavior: true}` は両方とも値なし（キャレットの下の
// 語で決める）。公開 aggregate（CPP-003）。
struct LspRenameTarget
{
    std::optional<LspRange> range;
    std::optional<std::string> placeholder;
};
} // namespace nenenib::core
