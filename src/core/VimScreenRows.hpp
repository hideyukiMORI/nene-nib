#pragma once

#include "VimDisplayNavigation.hpp"
#include "VimEditorView.hpp"
#include "VimLayoutNeed.hpp"
#include "VimRowLanding.hpp"
#include "VimRowRequest.hpp"
#include "VimRowSpan.hpp"
#include "VimScrollDirection.hpp"
#include "VimState.hpp"
#include "VisualPosition.hpp"

#include <cstddef>
#include <expected>

namespace nenenib::core
{
// 画面行の単位の画面移動（ADR 0144 の決定 3・5 とその追補）。画面の先頭（first_visible の行の
// first_row 番目の画面行）とキャレットの画面行を、画面行の列として数えて direction へ送る。
// 足りない行の画面行数と画面行の範囲は VimLayoutNeed で返し、状態は変えない。キャレットは送った後の
// 画面の中に収め、行き先の画面行の中で欲しい列（curswant）の仮想桁に着く（$ の後は画面行の末尾）。
// 先頭とキャレットのどちらも動かなければ何も変えない（失敗にしない）。

// 問いの答え（view.layout.spans の中で問いに当たる画面行）。無ければその問いを
// VimLayoutNeed で返す。画面行の問いの答えを引くのはこの 1 本（gj gk・画面移動・横送り・
// ADR 0145 の決定 3）。
[[nodiscard]] std::expected<VimRowSpan, VimLayoutNeed>
vim_row_span_of(const VimEditorView &view, const VimRowRequest &request);

// 1 画面（PgDn / PgUp / Ctrl-f / Ctrl-b）。先頭は本文の最初の画面行から最後の画面行が先頭になる
// までの間で amount 行まで動き、キャレットは先頭が実際に動いた数だけ動く。
[[nodiscard]] std::expected<VimDisplayNavigation, VimLayoutNeed>
vim_row_navigation(const VimState &state, const VimEditorView &view, VimScrollDirection direction,
                   std::size_t amount);

// 半画面（Ctrl-d / Ctrl-u・Vim の :help CTRL-D）。先頭は下へは最後の画面行が見えるまで、上へは
// 最初の画面行まで動き、キャレットは画面が送れない端でも amount 行（最後 / 最初の画面行で止める）
// 動く。
[[nodiscard]] std::expected<VimDisplayNavigation, VimLayoutNeed>
vim_row_half_navigation(const VimState &state, const VimEditorView &view,
                        VimScrollDirection direction, std::size_t amount);

// `gj` `gk` の着地（ADR 0144 の決定 3・4）。キャレットの画面行から direction へ amount 画面行
// だけ歩き（行の端を越えたら隣の論理行の最後 / 最初の画面行へ）、行き先の画面行の中で欲しい列に
// 着く。欲しい列の決め方は画面移動と同じ 1 本（決定 3 の追補・今の画面行の始まりからの桁）。
// 歩く前に欲しい列を「今の行の画面行数 × view.layout.row_columns − 1」に切り、着いた後もその値を
// 持ち越す（決定 10）。歩き切れなければ行ける所まで動いて失敗の印を付ける。`$` の後は画面行の
// 末尾。足りない文字組みの値は VimLayoutNeed。
[[nodiscard]] std::expected<VimRowLanding, VimLayoutNeed>
vim_row_landing(const VimState &state, const VimEditorView &view, VimScrollDirection direction,
                std::size_t amount);

// 格子の縦の位置合わせの画面の先頭（ADR 0146 の決定 5）。caret の画面行から上へ above 画面行だけ
// 歩いた（行・画面行）で、文書の最初の画面行で止める（上限は収めない）。足りない文字組みの値は
// VimLayoutNeed。
[[nodiscard]] std::expected<VisualPosition, VimLayoutNeed>
vim_row_aligned_top(const VimEditorView &view, Offset caret, std::size_t above);

// 格子の z+ の行（ADR 0146 の決定 9）。画面の先頭から画面の高さぶん前へ歩いた画面行（画面の最後の
// 画面行の次）の行で、文書の最後の画面行で止める（文書の末尾を越えれば最終行）。足りない文字組みの
// 値は VimLayoutNeed。
[[nodiscard]] std::expected<LineNumber, VimLayoutNeed>
vim_row_below_window(const VimEditorView &view);
} // namespace nenenib::core
