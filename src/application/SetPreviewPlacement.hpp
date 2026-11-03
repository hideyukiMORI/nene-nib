#pragma once

#include "PreviewPlacement.hpp"

namespace nenenib::application
{
// プレビューの見せ方を選ぶ（ADR 0155 の改訂 16）。使う人の操作で、F1 の一覧の「Markdown
// プレビュー（全面）」と Ex の `:preview full|side|off` が送る。今と同じ見せ方なら何も変えない。
struct SetPreviewPlacement
{
    PreviewPlacement placement;
};
} // namespace nenenib::application
