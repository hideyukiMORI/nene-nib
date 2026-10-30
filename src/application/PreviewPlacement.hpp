#pragma once

#include <cstdint>

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
} // namespace nenenib::application
