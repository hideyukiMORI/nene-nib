#pragma once

#include <cstddef>

namespace nenenib::core
{
// 全面プレビューの選択（ADR 0155 の改訂 18）。位置は PreviewPoint と同じく要素の番号と、要素の
// 見えている文字の中のバイト位置。begin は選び始めた端（押した点・Shift+矢印の起点）、end は動く
// 端で、文書の順とは限らない（順に並べるのは preview_selection_range）。begin と end が同じなら
// 空の選択。4 つの欄は単独で妥当な値なので公開 aggregate（CPP-003）。比較は非メンバー。
struct PreviewSelection
{
    std::size_t block_begin;
    std::size_t offset_begin;
    std::size_t block_end;
    std::size_t offset_end;
};

[[nodiscard]] constexpr bool operator==(const PreviewSelection &left,
                                        const PreviewSelection &right) noexcept
{
    return left.block_begin == right.block_begin && left.offset_begin == right.offset_begin &&
           left.block_end == right.block_end && left.offset_end == right.offset_end;
}
} // namespace nenenib::core
