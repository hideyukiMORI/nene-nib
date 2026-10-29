#pragma once

#include "VimNumberMiss.hpp"
#include "VimNumberSpan.hpp"
#include "VimNumberWindow.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::core
{
// `Ctrl-a` `Ctrl-x` の数（ADR 0015 の決定 14）。Vim 9.1 の do_addsub と vim_str2nr を
// `nrformats=bin,hex` で写した純関数で、窓の外を読まない（窓の端が行の端でなければ
// VimNumberMiss::window_short を返し、呼ぶ側が窓を広げる）。

// Vim が数を増減する回数の上限（normal.c は回数を 999999999 に貼り付ける・実測）。
inline constexpr std::uint64_t vim_number_count_limit = 999'999'999;

// NORMAL の `Ctrl-a`。キャレット（`window.at`）の上か右の最初の数。キャレットが 16 進・2 進の
// 桁や `0x` の上ならその数、直前の `-` は 10 進の負号。数が無ければ absent。
[[nodiscard]] std::expected<VimNumberSpan, VimNumberMiss>
vim_find_number(const VimNumberWindow &window) noexcept;

// VISUAL の 1 行。選択の始まり（`window.at`）から `length` バイトの中の最初の数。`-` は選択の
// 中にあるときだけ負号。`bounded` なら数を `length`
// の中で切る（文字単位と矩形・行単位は切らない）。
[[nodiscard]] std::expected<VimNumberSpan, VimNumberMiss>
vim_find_selected_number(const VimNumberWindow &window, std::size_t length, bool bounded) noexcept;

// `span` を `amount` だけ増やした（`subtract` なら減らした）文字列。桁数・`0x` `0b` の大小・
// 16 進の桁の大小を保ち、10 進は 64 bit の符号なしで回り込むと負号が反転する（Vim と同じ）。
[[nodiscard]] std::string vim_incremented_text(const VimNumberSpan &span, std::uint64_t amount,
                                               bool subtract);
} // namespace nenenib::core
