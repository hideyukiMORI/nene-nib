#pragma once

#include "EditorState.hpp"
#include "GridColorRange.hpp"
#include "Language.hpp"
#include "LexState.hpp"
#include "LineView.hpp"
#include "SyntaxStateCache.hpp"
#include "TextBuffer.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::application
{
// ADR 0156 の決定 8・9:
// 見えている行の色の区間を、見えている最初の行の手前から字句器を走らせて作る。 frame が枠ごとに 1
// 回だけ呼ぶ。字句器を持たない言語（core::has_lexer）では呼ばない（決定 14）。

// 覚えから続けてよい距離。これより先へ飛んだら遡りで作り直す。
inline constexpr std::size_t syntax_cache_reach = 4096;

// 見えている最初の行 key.line の行頭の状態。字句器を持たない言語（core::has_lexer）なら nullopt
// で、cache にも触れない。cache が key と同じ文書・言語で、手前の syntax_cache_reach 行以内に
// あればそこから、無ければ core::syntax_backfill_lines(言語) 行前の plain から状態だけを進め（区間
// を作らない。これより前で開いたブロックコメントは見えている行に届かない・限界）、cache を
// (key.line の行頭の状態) で置き換える。lexed に字句器へ通した行の数を足す。
[[nodiscard]] std::optional<core::LexState> syntax_begin(const core::TextBuffer &text,
                                                         const SyntaxStateCache &key,
                                                         std::optional<SyntaxStateCache> &cache,
                                                         std::size_t &lexed);

// 見えている 1 行の区間を view.colors に置き、次の行へ渡す状態を返す（in が nullopt なら何もせず
// nullopt）。巨大な行の窓は区間を付けず、行の全体を読まずに plain へ戻す（限界）。IME の差し込み
// 行は本文の区間を差し込みの位置で割り、後ろを差し込んだ code point の数だけずらす（差し込みの
// 範囲は色なし・#839）。lexed に 1 を足す。
[[nodiscard]] std::optional<core::LexState> colored_line(core::Language language,
                                                         const std::optional<core::LexState> &in,
                                                         LineView &view, std::size_t &lexed);

// 格子の 1 行（#839・ADR 0156 の追補）。行の区間を本文の位置へ写して colors に置き、次の行へ渡す
// 状態を返す（in が nullopt なら何もせず nullopt・呼び手は in があるとき字句器へ通した行を 1
// 数える）。 言語は state の文書の言語。巨大な行は colored_line の窓と同じく区間を付けず plain
// へ戻す。
[[nodiscard]] std::optional<core::LexState>
grid_colored_line(const EditorState &state, const std::optional<core::LexState> &in,
                  core::LineNumber line, std::vector<GridColorRange> &colors);
} // namespace nenenib::application
