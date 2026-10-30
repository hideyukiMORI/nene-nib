#pragma once

#include <cstdint>

namespace nenenib::core
{
// ステータスバーで当たる部位の閉じた一覧。モードトグルの 2 つ（ADR 0008）と、右の 4 項目のうち
// 文字コードと改行の 2 つ（ADR 0010 の追補 #1049 の決定 2 の (a)・押すと保存の形の面を開く）。
enum class StatusBarHit : std::uint8_t
{
    toggle_ordinary,
    toggle_vim,
    encoding,
    line_ending,
    none
};
} // namespace nenenib::core
