#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// 要素の layout の中に画像を置く箱（施主決定 D79・ADR 0155 の追補 62・#1439）。begin と
// length は要素の見えている文字の中の「[画像: alt]」の範囲（layout の UTF-16 の位置）、
// width と height は core::image_placement が決めた描く大きさ（DIP・拡大しない・枠の本文の
// 幅に縮めるだけ）。PreviewTextLayouts の保持の鍵に入り（画素は入れない）、layout はこの範囲
// を同じ大きさの inline の物にする。4 つは互いに独立して妥当なので公開 aggregate（CPP-003）。
struct PreviewInlineBox
{
    std::uint32_t begin;
    std::uint32_t length;
    std::uint32_t width;
    std::uint32_t height;
};

[[nodiscard]] constexpr bool operator==(PreviewInlineBox left, PreviewInlineBox right) noexcept
{
    return left.begin == right.begin && left.length == right.length && left.width == right.width &&
           left.height == right.height;
}
} // namespace nenenib::ui::win32
