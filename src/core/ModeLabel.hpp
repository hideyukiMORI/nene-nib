#pragma once

#include "EditMode.hpp"
#include "VimMode.hpp"

#include <string_view>

namespace nenenib::core
{
// ステータスバーに出す現在モードの文字。Vim のときは Vim のモードがそのまま名前になる
// （採用案 第 1 節・D15 / ADR 0012 の決定 10）。insert_paused は INSERT の `Ctrl-o` の命令を待つ
// 間（VimState.insert_restart が待ちか due・vim_insert_paused）で、NORMAL と VISUAL を `(insert)`
// つきで出す（ADR 0182 の決定 9）。
[[nodiscard]] std::string_view mode_label(EditMode mode, VimMode vim, bool insert_paused) noexcept;
} // namespace nenenib::core
