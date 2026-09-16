#pragma once

#include "Composition.hpp"

namespace nenenib::application
{
// IME の変換中の文字列が変わった（ADR 0014 の決定 3）。本文も履歴もスクロールも動かず、
// 変換中の文字列だけが EditorState の外側に置き換わる（ARC-004）。
struct ComposeText
{
    core::Composition composition;
};
} // namespace nenenib::application
