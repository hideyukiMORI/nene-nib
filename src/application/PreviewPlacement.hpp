#pragma once

#include <cstdint>
#include <utility>

namespace nenenib::application
{
// Markdown のプレビューの見せ方（ADR 0155 の改訂 15）。off は出さない、side はもう 1 つの枠
// （SplitState.other・決定 1）、full は本文の面の全部（EditorState の full_preview・gutter も本文も
// 描かず、タブの帯とステータスバーは残す）。PreviewView はどちらでも同じ型で、同時に 2 つは
// 持たない。値が増えたら写し先の switch がコンパイルで落ちる（CPP-002）。
enum class PreviewPlacement : std::uint8_t
{
    off,
    side,
    full
};

// Ctrl+Shift+M（操作 toggle_preview・意図 TogglePreview）の後の見せ方（施主決定 D77・ADR 0155 の
// 追補 #1354）。押すたびに off → side → full → off と巡る。`:preview full|side|off` と F1 の
// 「Markdown プレビュー（全面）」と全面の間の Esc はこの関数を通らない（見せ方を名指しで置く）。
[[nodiscard]] constexpr PreviewPlacement cycled_preview_placement(PreviewPlacement now) noexcept
{
    switch (now)
    {
    case PreviewPlacement::off:
        return PreviewPlacement::side;
    case PreviewPlacement::side:
        return PreviewPlacement::full;
    case PreviewPlacement::full:
        return PreviewPlacement::off;
    }
    std::unreachable();
}
} // namespace nenenib::application
