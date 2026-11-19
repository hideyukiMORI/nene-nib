#pragma once

#include <cstdint>
#include <utility>

namespace nenenib::core
{
// 囲みのコードの要素が、元の囲みの切れの列のどこか（ADR 0151 の追補 2026-11-19・#1863）。
// preview_code_piece_lines 行を超える囲みは続く要素の列（first・middle…・last）で出し、
// それ以下の囲みとコード以外の要素は only。面は切れの列で 1 枚に見える（first は上の余白と角、
// last は下の余白と角、middle は余白と角なし・続く切れの間の空きは 0）。
enum class PreviewCodePiece : std::uint8_t
{
    only,
    first,
    middle,
    last
};

// 面の上の余白と角を持つ切れ（囲みの最初）か。
[[nodiscard]] constexpr bool preview_code_piece_opens(PreviewCodePiece piece) noexcept
{
    switch (piece)
    {
    case PreviewCodePiece::only:
    case PreviewCodePiece::first:
        return true;
    case PreviewCodePiece::middle:
    case PreviewCodePiece::last:
        return false;
    }
    std::unreachable();
}

// 面の下の余白と角を持つ切れ（囲みの最後）か。
[[nodiscard]] constexpr bool preview_code_piece_closes(PreviewCodePiece piece) noexcept
{
    switch (piece)
    {
    case PreviewCodePiece::only:
    case PreviewCodePiece::last:
        return true;
    case PreviewCodePiece::first:
    case PreviewCodePiece::middle:
        return false;
    }
    std::unreachable();
}
} // namespace nenenib::core
