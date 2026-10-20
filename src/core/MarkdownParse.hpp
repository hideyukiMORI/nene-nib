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
} // namespace nenenib::core
