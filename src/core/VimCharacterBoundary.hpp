#pragma once

#include "Offset.hpp"

#include <string_view>

namespace nenenib::core
{
// Vim の「1 文字」の境（ADR 0053）。1 文字は code point 1 つと、その直後に続く幅 0
// （`display_width(cp) == DisplayWidth::zero`）の code point の列である。判定は仮想桁の表の
// 1 つだけを使い、文字を歩く関数はこの 1 対だけ（ARC-001）。utf8 は検証済みの 1 行の内容で、
// 0 はどの code point でも文字の先頭（行頭の孤立した結合文字はそれ自身が文字の先頭）。
// `next_code_point` / `previous_code_point` はバイト列の走査のまま変えない。

// at から始まる文字の終わり（次の文字の先頭）。末尾では動かない。
[[nodiscard]] Offset vim_character_end(std::string_view utf8, Offset at) noexcept;

// at の手前で終わる文字の先頭。at が文字の途中ならその文字の先頭。先頭では動かない。
[[nodiscard]] Offset vim_character_start(std::string_view utf8, Offset at) noexcept;
} // namespace nenenib::core
