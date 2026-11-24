#pragma once

#include "JsonValue.hpp"
#include "LspAnswerFailure.hpp"
#include "LspRenameTarget.hpp"

#include <expected>
#include <optional>

namespace nenenib::core
{
// `textDocument/prepareRename` の答え（ADR 0194 の決定 7）。null と `{defaultBehavior: false}` は
// 「ここは名前を変えられない」で値なし。`Range` は range だけ、`{range, placeholder}` は両方
// （placeholder の欠けた形は range だけ）、`{defaultBehavior: true}` は両方とも値なし。ほかの形と、
// 読めない range は invalid_result。OS に触れない純関数。
[[nodiscard]] std::expected<std::optional<LspRenameTarget>, LspAnswerFailure>
lsp_rename_target(const JsonValue &result);
} // namespace nenenib::core
