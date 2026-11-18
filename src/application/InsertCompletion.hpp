#pragma once

#include "CompletionDirection.hpp"
#include "CompletionList.hpp"
#include "EditorState.hpp"

namespace nenenib::application
{
// INSERT の Ctrl-n / Ctrl-p の候補の列（ADR 0169 の決定 1・2）。アクティブな文書の本文とキャレット
// と語の表、読み込み済みの他のタブの表（帯の順）から core の completion_list の 1 本で作る。
// 面と鍵（#1008 の段 B）はここを呼ぶ。本文を走るのは呼ぶときの 1 回だけ（窓の大きさは
// core::completion_scan_bytes）。
[[nodiscard]] core::CompletionList insert_completion_list(const EditorState &state,
                                                          core::CompletionDirection direction);
} // namespace nenenib::application
