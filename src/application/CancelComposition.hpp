#pragma once

namespace nenenib::application
{
// 変換を捨てる（変換中の Esc・WM_IME_ENDCOMPOSITION）。本文も履歴も変わらない
// （ADR 0014 の決定 3）。変換していなければ何も起きない。
struct CancelComposition
{
};
} // namespace nenenib::application
