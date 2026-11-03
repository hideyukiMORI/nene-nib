#pragma once

#include "MarkdownDocument.hpp"
#include "MarkdownFailure.hpp"
#include "MarkdownStop.hpp"

#include <cstddef>
#include <expected>
#include <string_view>

namespace nenenib::core
{
// Markdown の本文を md4c（CommonMark・拡張の flag は立てない）で読み、leaf ブロックの平らな列に
// 写す純関数（ADR 0151 の決定 5〜8）。md4c の型は公開しない。stop は md4c のコールバックのたびに
// 問い、true なら打ち切って MarkdownFailure::stopped を返す。
[[nodiscard]] std::expected<MarkdownDocument, MarkdownFailure>
parse_markdown(std::string_view text, const MarkdownStop &stop);

// md4c の大きさの型（unsigned）に入るか。入らない本文は parse_markdown が too_large で断る。
[[nodiscard]] bool markdown_size_fits(std::size_t size) noexcept;

// プレビューが解析を頼む本文の上限（ADR 0155 の決定 3）。これを超える本文は要求せず、枠に
// 「プレビューは 16 MiB までです」を出す。parse_markdown 自身の上限は markdown_size_fits。
inline constexpr std::size_t preview_byte_limit = std::size_t{16} * 1024 * 1024;
// 印刷を頼む本文の上限（ADR 0177 の決定 7・プレビューと同じ所の定数）。これを超える本文は port を
// 呼ばず「印刷は 16 MiB までです」を出す。ページの数には上限を置かない（OS のスプールに任せる）。
inline constexpr std::size_t print_byte_limit = std::size_t{16} * 1024 * 1024;
} // namespace nenenib::core
