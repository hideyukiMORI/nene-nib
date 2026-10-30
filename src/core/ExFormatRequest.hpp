#pragma once

#include "DocumentFormat.hpp"
#include "DocumentFormatKind.hpp"

#include <optional>

namespace nenenib::core
{
// `:set fileencoding=<名前>` `:set fenc=<名前>` `:set fileformat=<名前>` `:set ff=<名前>` と、
// それぞれの `?`（ADR 0010 の追補 #1049 の決定 2 の (b)）。chosen は選んだ値で、空は kind の今の
// 値を知らせる問い。今の値は文書を持つ controller が答える（ExLanguageRequest と同じ形）。
struct ExFormatRequest
{
    DocumentFormatKind kind;
    std::optional<DocumentFormat> chosen;
};
} // namespace nenenib::core
