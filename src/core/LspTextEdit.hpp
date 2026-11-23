#pragma once

#include "LspRange.hpp"

#include <string>

namespace nenenib::core
{
// LSP の `TextEdit` 1 つ（ADR 0191 の決定 3・7）。range は要求を出した版の本文の上の位置（単位は
// 答えた席の positionEncoding）、new_text は置き換える本文。公開 aggregate（CPP-003）。
struct LspTextEdit
{
    LspRange range;
    std::string new_text;
};
} // namespace nenenib::core
