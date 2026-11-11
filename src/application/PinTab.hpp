#pragma once

#include <cstddef>

namespace nenenib::application
{
// 帯の位置 index のタブの題名のダブルクリック（ADR 0179 の決定 4-4）。SwitchTab{index} と同じ
// 切り替えをして、切り替えが成った（アクティブがそのタブの退避番号になった）ときだけ、そのタブが
// プレビューのタブなら固定する（印を消すだけで文書は変えない）。範囲の外なら何もしない。使う人の
// 操作なので、detail::is_signal にも keeps_tab_walk にも begin_intent の表にも書かない。
struct PinTab
{
    std::size_t index;
};
} // namespace nenenib::application
