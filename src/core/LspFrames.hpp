#pragma once

#include "LspFrameFailure.hpp"

#include <cstddef>
#include <expected>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 1 つの枠の本文の上限（ADR 0160 の決定 3・L-4 の文書の大きさと同じ桁）と、見出しの上限。
// 見出しの上限は空行の前までの長さで、空行を含めた長さはこれ＋4 バイトまで（#1165）。
inline constexpr std::size_t lsp_body_limit = std::size_t{16} * 1024 * 1024;
inline constexpr std::size_t lsp_header_limit = std::size_t{8} * 1024;

// 揃った枠の本文（入力を指す view）と、揃った最後の枠の終わりのバイト位置。adapter は consumed
// だけ前を捨て、残り（途中の枠）に次に読んだバイト列を繋いでまた渡す。
struct LspFrames
{
    std::vector<std::string_view> bodies;
    std::size_t consumed;
};

// LSP の base protocol の枠を切り出す唯一の入口（ADR 0160 の決定 3）。OS に触れない純関数。
// 見出しは `名前: 値` の行の列を空行（`\r\n\r\n`）で閉じ、`Content-Length` だけを読む（名前の
// 大文字小文字を区別しない・`Content-Type` とほかの名前は読み飛ばす）。本文はその後ろの N バイト。
// 途中で尽きた枠は返さず、consumed はそこより前で止まる。見出しの誤りと上限の超えは閉じた失敗。
[[nodiscard]] std::expected<LspFrames, LspFrameFailure> lsp_frames(std::string_view bytes);
} // namespace nenenib::core
