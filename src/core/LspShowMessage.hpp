#pragma once

#include "DisplayText.hpp"
#include "LspMessageType.hpp"
#include "LspNotification.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
// サーバーの知らせの通知の method（LSP 3.17・ADR 0161 の決定 5）。
inline constexpr std::string_view lsp_show_message_method = "window/showMessage";
inline constexpr std::string_view lsp_log_message_method = "window/logMessage";

// `window/showMessage` の通知 → 知らせの 1 行（ADR 0161 の決定 5）。params の type が 1（Error）か
// 2（Warning）のときだけ、message を notice_line で 1 行（256 バイト）にして返す。3（Info）・
// 4（Log）・欠けた欄・ほかの method（`window/logMessage` を含む）は値なし（捨てる）。
[[nodiscard]] std::optional<DisplayText> lsp_shown_notice(const LspNotification &notification);

// `window/showMessage` と `window/logMessage` の通知 → 記録に置く種類と文（ADR 0160 の
// 2026-11-22 の追補・LSP 段 S8）。type は整数でなければならず、1〜4 のほかの数は log。文は手を
// 加えない（行に分けて直すのは記録の側）。ほかの method・type か message が欠けた・形の違う通知は
// 値なし。
[[nodiscard]] std::optional<std::pair<LspMessageType, std::string>>
lsp_logged_message(const LspNotification &notification);
} // namespace nenenib::core
