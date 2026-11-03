#pragma once

#include <cstddef>

namespace nenenib::core
{
// 全面プレビューの描いた文字の上の 1 点（ADR 0155 の改訂 18）。block は MarkdownDocument の要素の
// 番号、offset はその要素の見えている文字（preview_shown_text）の中のバイト位置（code point の
// 境目）。どちらも単独で妥当な値なので公開 aggregate（CPP-003）。比較は非メンバー。
struct PreviewPoint
{
    std::size_t block;
    std::size_t offset;
};

[[nodiscard]] constexpr bool operator==(const PreviewPoint &left,
                                        const PreviewPoint &right) noexcept
{
    return left.block == right.block && left.offset == right.offset;
}

// 文書の順で left が right より前か。
[[nodiscard]] constexpr bool preview_point_before(const PreviewPoint &left,
                                                  const PreviewPoint &right) noexcept
{
    return left.block < right.block || (left.block == right.block && left.offset < right.offset);
}
} // namespace nenenib::core
