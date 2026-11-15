#pragma once

#include <cstdint>

namespace nenenib::core
{
// 命令の列（ADR 0184 の決定 3）の命令の種類。
// atom は原子 1 つ（食べる原子は 1 文字を食べ、幅の無い原子は錨を確かめる・
// 原子の量は split と jump に開いてある）、split は 2 つの道（first が先）、
// jump は跳び、open と close は群の端（照合は素通りし、
// 群の範囲を引く道の探索だけが読む）、match_begin と match_end は `\zs` と `\ze`
// （当たりの始まりと終わりを置き直す・照合器が候補ごとに運ぶ・ADR 0184 の決定 5）、accept
// は当たり。
enum class VimPatternOpcode : std::uint8_t
{
    atom,
    split,
    jump,
    open,
    close,
    match_begin,
    match_end,
    accept
};
} // namespace nenenib::core
