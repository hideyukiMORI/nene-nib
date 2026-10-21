#pragma once

#include "Language.hpp"
#include "LexState.hpp"
#include "LineView.hpp"
#include "SyntaxStateCache.hpp"
#include "TextBuffer.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::application
{
// ADR 0156 の決定 8・9:
// 見えている行の色の区間を、見えている最初の行の手前から字句器を走らせて作る。 frame が枠ごとに 1
// 回だけ呼ぶ。字句器を持たない言語（core::has_lexer）では呼ばない（決定 14）。

// 覚えが無いときに遡る行数（Vim の c_minlines の型）。これより前で開いたブロックコメントは
// 見えている行に届かない（限界）。
inline constexpr std::size_t syntax_backfill_lines = 200;
// 覚えから続けてよい距離。これより先へ飛んだら遡りで作り直す。
inline constexpr std::size_t syntax_cache_reach = 4096;

// 見えている最初の行 key.line の行頭の状態。字句器を持たない言語（core::has_lexer）なら nullopt
// で、cache にも触れない。cache が key と同じ文書・改訂・言語で、手前の syntax_cache_reach 行
// 以内にあればそこから、無ければ syntax_backfill_lines 行前の plain から状態だけを進め（区間を
// 作らない）、cache を (key.line の行頭の状態) で置き換える。lexed に字句器へ通した行の数を足す。
[[nodiscard]] std::optional<core::LexState> syntax_begin(const core::TextBuffer &text,
                                                         const SyntaxStateCache &key,
                                                         std::optional<SyntaxStateCache> &cache,
                                                         std::size_t &lexed);

// 見えている 1 行の区間を view.colors に置き、次の行へ渡す状態を返す（in が nullopt なら何もせず
// nullopt）。巨大な行の窓は区間を付けず、行の全体を読まずに plain へ戻す（限界）。IME の差し込み
// 行は区間を付けず状態だけ進める。lexed に 1 を足す。
[[nodiscard]] std::optional<core::LexState> colored_line(core::Language language,
                                                         const std::optional<core::LexState> &in,
                                                         LineView &view, std::size_t &lexed);
} // namespace nenenib::application
