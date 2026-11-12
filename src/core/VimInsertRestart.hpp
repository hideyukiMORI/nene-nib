#pragma once

#include "LineNumber.hpp"
#include "VimInsertRestartPhase.hpp"

namespace nenenib::core
{
// INSERT の `Ctrl-o` が立てる「始め直す印」（ADR 0182 の決定 1）。line と at_line_end は Vim の
// o_lnum と ins_at_eol（始め直す位置の規則・決定 2）、arrow_used は `Ctrl-o` の前の INSERT で
// まだ何も打っていなかったか（`<C-o>.` が今の変更を繰り返す側・決定 6）、replayed は `Ctrl-o`
// が再生の中の鍵だったか（再生の中の `i` が印を消すか・決定 4）。
struct VimInsertRestart
{
    LineNumber line;
    bool at_line_end;
    bool arrow_used;
    bool replayed;
    VimInsertRestartPhase phase;
};
} // namespace nenenib::core
