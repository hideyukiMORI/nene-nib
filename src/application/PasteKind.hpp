#pragma once

#include "Language.hpp"

#include <cstdint>

namespace nenenib::application
{
// Ctrl+V（通常モードと Vim INSERT の OS の貼り付け）が何を貼るか（ADR 0157 の決定 1）。
enum class PasteKind : std::uint8_t
{
    // CF_UNICODETEXT の文字だけ（今までどおり）。
    text,
    // クリップボードの HTML を Markdown に変換して貼る。
    markdown_from_html,
    // HTML の fragment をそのまま貼る。
    html_as_is
};

// 文書の言語とクリップボードに HTML があるか → 貼り方。判定はこの 1 か所（決定 1）。
[[nodiscard]] PasteKind paste_kind_of(core::Language language, bool has_html) noexcept;
} // namespace nenenib::application
