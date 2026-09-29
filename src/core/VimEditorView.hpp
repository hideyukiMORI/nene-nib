#pragma once

#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimKeySource.hpp"
#include "VimTabs.hpp"
#include "VimViewport.hpp"

namespace nenenib::core
{
// Vim の 1 打鍵が読む editor の現在値。本文と選択は借用し、表示領域は値で受ける。
// どれも保存せず、状態の所有者を増やさない（ADR 0019 の決定 1）。
struct VimEditorView
{
    const TextBuffer &text;
    const Selection &selection;
    VimViewport viewport;
    // 鍵の出どころ（ADR 0046 の決定 2）。controller が再生の中だけ replayed にする。
    VimKeySource source = VimKeySource::typed;
    // タブの本数と今の位置（ADR 0057 の決定 2）。既定はタブ 1 本の今の位置 0。
    VimTabs tabs = VimTabs{0, 1};
};
} // namespace nenenib::core
