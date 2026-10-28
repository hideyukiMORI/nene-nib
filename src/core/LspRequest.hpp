#pragma once

#include "JsonValue.hpp"
#include "LspId.hpp"

#include <string>

namespace nenenib::core
{
// サーバーから来た要求（`id` と `method` を持つ・ADR 0160 の決定 5）。Nib は今は答えを持たず、
// lsp_method_not_found_body で断る。params が無ければ JsonNull。
struct LspRequest
{
    LspId id;
    std::string method;
    JsonValue params;
};
} // namespace nenenib::core