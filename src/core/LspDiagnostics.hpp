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

// 1 件の文の上限（バイト・#1939）。超えた分は code point を割らない所で切る。上限の 1,000 件と
// 掛けても 1 文書で 4 MiB に収まる。札は hover と同じく 12 行で切る（hover_tip_text）。
inline constexpr std::size_t lsp_diagnostic_message_limit = 4096;

// source と文字列の code の上限（バイト）。超えたら欄が無いのと同じ（空）。
inline constexpr std::size_t lsp_diagnostic_label_limit = 128;

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
// VS Code と同じ）。message は文の全部（lsp_diagnostic_message_limit まで）、source と code も
// 読む（#1939・決定 9 の追補）。`relatedInformation` `tags` ほかの欄は読まない（決定
// 9）。残した件が lsp_diagnostics_limit に達したら残りは捨てる。
[[nodiscard]] std::expected<PublishedDiagnostics, LspDiagnosticsFailure>
lsp_published_diagnostics(const LspNotification &notification, const TextBuffer &text,
                          LspPositionEncoding encoding);
} // namespace nenenib::core
