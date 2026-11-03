#pragma once

#include "LspDiagnosticsFailure.hpp"
#include "LspNotification.hpp"
#include "LspPositionEncoding.hpp"
#include "LspRange.hpp"
#include "OffsetRange.hpp"
#include "PublishedDiagnostics.hpp"
#include "TextBuffer.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string_view>

namespace nenenib::core
{
// 1 文書の診断の上限（ADR 0161 の決定 2・限界）。超えた分は捨てて件数に `+` を付ける。
inline constexpr std::size_t lsp_diagnostics_limit = 1000;

// 診断を運ぶ通知の method（LSP 3.17）。
inline constexpr std::string_view lsp_publish_diagnostics_method =
    "textDocument/publishDiagnostics";

// LSP の Range → 本文のバイト位置の唯一の写し（ADR 0161 の決定 2）。両端は lsp_offset_of の 1 本で
// 写す（桁が行の外なら行末へ切り詰める）。行が本文の外の端は本文の末尾へ切り詰める（start も end
// も外なら末尾の空の区間＝EOF の診断として残す）。写した start が end より後ろなら値なし。
[[nodiscard]] std::optional<OffsetRange>
lsp_range_to_offsets(const TextBuffer &text, const LspRange &range, LspPositionEncoding encoding);

// `textDocument/publishDiagnostics` の通知 → 文書の値の診断（ADR 0161 の決定 2・5）。text は診断を
// 置く文書の今の本文。各 range は lsp_range_to_offsets で写し、写せない件・range / message の
// 欠けた件・1〜4 の外の severity の件は捨てる。severity が無ければ error（LSP は client に任せる・
// VS Code と同じ）。message は最初の改行（LF か CR）の前までの 1 行。`relatedInformation` ほかの
// 欄は読まない（決定 9）。残した件が lsp_diagnostics_limit に達したら残りは捨てる。
[[nodiscard]] std::expected<PublishedDiagnostics, LspDiagnosticsFailure>
lsp_published_diagnostics(const LspNotification &notification, const TextBuffer &text,
                          LspPositionEncoding encoding);
} // namespace nenenib::core
