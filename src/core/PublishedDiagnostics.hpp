#pragma once

#include "Diagnostic.hpp"
#include "DiagnosticsCount.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::core
{
// `textDocument/publishDiagnostics` 1 通を本文の位置へ写した値（ADR 0161 の決定 2）。version は
// サーバーが添えた文書の版（無ければ値なし）。diagnostics は届いた順で、上限を超えた分を捨てたら
// count が truncated（件数に `+` を付ける）。
struct PublishedDiagnostics
{
    std::string uri;
    std::optional<std::uint64_t> version;
    std::vector<Diagnostic> diagnostics;
    DiagnosticsCount count;
};
} // namespace nenenib::core
