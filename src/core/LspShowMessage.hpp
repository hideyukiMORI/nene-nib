#pragma once

#include "DisplayText.hpp"
#include "LspNotification.hpp"

#include <optional>
#include <string_view>

namespace nenenib::core
{
// サーバーの知らせの通知の method（LSP 3.17・ADR 0161 の決定 5）。
inline constexpr std::string_view lsp_show_message_method = "window/showMessage";
inline constexpr std::string_view lsp_log_message_method = "window/logMessage";

// `window/showMessage` の通知 → 知らせの 1 行（ADR 0161 の決定 5）。params の type が 1（Error）か
// 2（Warning）のときだけ、message を notice_line で 1 行（256 バイト）にして返す。3（Info）・
// 4（Log）・欠けた欄・ほかの method（`window/logMessage` を含む）は値なし（捨てる）。
[[nodiscard]] std::optional<DisplayText> lsp_shown_notice(const LspNotification &notification);
} // namespace nenenib::core
