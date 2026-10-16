#pragma once

#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "VimPattern.hpp"

namespace nenenib::application
{
// 見えている行（格子では画面行の切れ端）に書き足す選択と検索の当たりの入力（ADR 0037 / 0095）。
// bounds は切れ端の本文の範囲（最後の切れ端だけ改行を含む）、range は塗る選択、pattern は検索の
// パターン（無ければ nullptr・visible_lines の同期呼出しの間だけ借用）、caret は今の一致を決める位置。
struct LineDecoration
{
    core::OffsetRange bounds;
    core::OffsetRange range;
    const core::VimPattern *pattern;
    core::Offset caret;
};
} // namespace nenenib::application
