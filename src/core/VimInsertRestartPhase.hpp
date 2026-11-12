#pragma once

#include <cstdint>

namespace nenenib::core
{
// INSERT の `Ctrl-o` の印の段（ADR 0182）。waiting は NORMAL の命令 1 つを待つ間（Vim の
// restart_edit が立って normal_cmd がまだ終わっていない）、due は命令が終わって始め直す時が来た
// 印（controller が vim_restarted_insert を呼ぶ）、restarted は始め直した INSERT でまだ何も
// 打っていない間（Vim の arrow_used・`.` の記録を `1i` から取り直す）。
enum class VimInsertRestartPhase : std::uint8_t
{
    waiting,
    due,
    restarted
};
} // namespace nenenib::core
