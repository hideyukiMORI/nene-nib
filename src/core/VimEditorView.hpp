#pragma once

#include "Selection.hpp"
#include "TextBuffer.hpp"
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
};
} // namespace nenenib::core
