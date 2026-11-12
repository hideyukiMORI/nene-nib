#pragma once

#include <cstdint>

namespace nenenib::core
{
// 照合器が扱う原子の閉じた一覧（ADR 0032 の決定 4）。magic の部分集合だけで、
// 未対応の構文は原子にならず閉じた失敗（VimPatternFailure）で拒否される。
enum class VimPatternAtomKind : std::uint8_t
{
    // 1 つの code point そのもの。
    literal,
    // `.`（改行以外の 1 文字。一致は行をまたがない）。
    any,
    // `[...]` `[^...]` と `\d \D \w \W \s \S`。
    set,
    // 結合文字だけの原子（#1506）。基底の literal の直後でない結合文字の列（パターンの先頭・
    // `.` の直後で `.` は消える・`*` や幅の無い原子の後ろ）。基底を問わず、列の結合文字を全部
    // （順は問わない）持つ 1 文字に当たり、その 1 文字を丸ごと食べる（Vim 9.1 の NFA_COMPOSING
    // の基底を無視する形・`:h patterns-composing`）。結合文字は set と同じ区間の列に持つ。
    composing,
    // 基底と結合文字の列の 1 文字（ADR 0184 の決定 2）。量の付いた `é`（基底の literal と続く
    // 結合文字の literal）を parse が 1 原子に畳んだもの（Vim 9.1 の NFA_COMPOSING と同じく量は
    // 1 文字の全体に掛かる・`é\+` は `éé` に当たる・実測）。本文の 1 文字が基底と同じ結合文字の
    // 列そのもの（大小を畳まない）のときだけ当たり、その 1 文字を丸ごと食べる。結合文字は set と
    // 同じ区間の列に順に持つ。量の無い `é` は今どおり literal の列で比べる（決定 1）。
    cluster,
    // `^`（パターンの先頭でだけ）。
    line_start,
    // `$`（パターンの末尾でだけ）。
    line_end,
    // `\<`。
    word_start,
    // `\>`。
    word_end
};
} // namespace nenenib::core
