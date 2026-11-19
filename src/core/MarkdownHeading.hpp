#pragma once

#include <cstddef>
#include <string>

namespace nenenib::core
{
// 文書の見出しの 1 つとその名札（slug・ADR 0151 の追補 2026-11-19 の段 L3・#1869）。block は
// MarkdownDocument の blocks の番号（見出しの要素）、slug は heading_slugs が GitHub の形で作った
// 名札（文書の中で 2 つ目からの同じ名札は `-1` `-2` … の付いた形）。比較は非メンバー（CPP-003）。
struct MarkdownHeading
{
    std::size_t block;
    std::string slug;
};

[[nodiscard]] inline bool operator==(const MarkdownHeading &left, const MarkdownHeading &right)
{
    return left.block == right.block && left.slug == right.slug;
}
} // namespace nenenib::core
