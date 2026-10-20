#pragma once

#include <cstdint>

namespace nenenib::core
{
// 入力行の 1 つの編集（ADR 0032 の決定 1）。補完の 2 つと一覧の 4 つ（1
// 画面ぶん前・次・先頭・末尾・ ADR 0150 の決定
// 2）は候補を持つ型だけが動かし、文字列とキャレットの規則としては何もしない。履歴の前・次
// （#725・ADR 0022 決定 2 の追補）は InputHistory を渡される `:` と検索の行だけが動かす。
enum class CommandEdit : std::uint8_t
{
    left,
    right,
    home,
    end,
    backspace,
    erase,
    complete_next,
    complete_previous,
    page_previous,
    page_next,
    list_first,
    list_last,
    history_previous,
    history_next
};
} // namespace nenenib::core
