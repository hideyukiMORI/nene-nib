#pragma once

namespace nenenib::application
{
// INSERT の `Ctrl-o` の印で INSERT を始め直す 1 歩（ADR 0182 の決定 2・8）。鍵ではないので engine
// は vim_restarted_insert で受け、`.` とマクロの記録に積まない。controller が印の due の鍵の効果を
// 写した後・入力行を閉じた後・クリックの後に流す。
struct InsertRestartRequest
{
};
} // namespace nenenib::application
