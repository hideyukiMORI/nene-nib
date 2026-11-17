#pragma once

#include <cstdint>

namespace nenenib::application
{
// タブの札の段（#1789・ADR 0056 の #1789 の追補）。どのタブかは載せた要素（EditorState::hovered）
// が持ち、ここは覚えない。hidden は出していない・待っていない、waiting は載せて待っている（ui の
// タイマーが動いている）、shown は札を出している。消す意図のあと同じタブに載ったままなら hidden の
// まま（離れて載せ直すまで出さない・Windows のツールチップと同じ）。
enum class TabTipPhase : std::uint8_t
{
    hidden,
    waiting,
    shown
};
} // namespace nenenib::application
