#pragma once

#include <cstdint>

namespace nenenib::core
{
// 設定の面に並ぶ機能（ADR 0170 の決定 1・2026-11-18 の追補 1）。今の製品に在る機能だけを値に
// する（まだ無い機能は作る PR が足す）。表 feature_costs はこの宣言の順に 1 行ずつで、値を足すと
// 表の static_assert と写し先の switch がコンパイルで落ちる（CPP-002）。
enum class EditorFeature : std::uint8_t
{
    wrap,
    guide,
    autoindent,
    matchparen,
    easymotion,
    explorer,
    minimap,
    start_screen,
    lsp,
    syntax,
    // キャレットの動き（ADR 0192・#1933・自分の鍵 caret_motion）。
    caret_motion,
    // 打ったときの粒（ADR 0192 の追補・#1979・自分の鍵 caret_sparks）。
    caret_sparks,
    // インデントガイド（ADR 0193 の決定 3・#1975・`features=indentguides`）。
    indent_guides
};
} // namespace nenenib::core
