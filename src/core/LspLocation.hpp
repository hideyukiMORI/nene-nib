#pragma once

#include "LspRange.hpp"

#include <string>

namespace nenenib::core
{
// 定義の答えの場所 1 つ（ADR 0190 の決定 4）。`Location` の uri と range、`LocationLink` の
// targetUri と targetSelectionRange（無ければ targetRange）。uri → パスは file_path_of_uri の
// 1 本で写す。公開 aggregate（CPP-003）。
struct LspLocation
{
    std::string uri;
    LspRange range;
};
} // namespace nenenib::core
