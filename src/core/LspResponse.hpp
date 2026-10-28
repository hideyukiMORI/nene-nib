#pragma once

#include "JsonValue.hpp"
#include "LspError.hpp"
#include "LspId.hpp"

#include <expected>

namespace nenenib::core
{
// Nib の要求への答え（ADR 0160 の決定 5）。`result` か `error` のどちらか。`result` が無い答えは
// JsonNull の result として読む。
struct LspResponse
{
    LspId id;
    std::expected<JsonValue, LspError> result;
};
} // namespace nenenib::core