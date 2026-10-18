#pragma once

#include "VimAction.hpp"
#include "VimDisplayNavigation.hpp"
#include "VimEditorView.hpp"
#include "VimLayoutNeed.hpp"
#include "VimScreenPlace.hpp"
#include "VimState.hpp"
#include "VisualPosition.hpp"

#include <expected>
#include <optional>

namespace nenenib::core
{
// 表示領域（VimViewport）を読む鍵か（ADR 0129 の決定 1）。画面の行に関わる H M L と
// Ctrl-d/u/f/b（PgUp/PgDn も同じ動作）・横送り・縦の位置合わせ（ADR 0146 の決定 2）。
[[nodiscard]] bool vim_reads_viewport(VimAction action) noexcept;

// 純粋な画面移動の解決。行数か行の中の画面行が足りなければ、その問いを返し、状態は変えない。
// 格子（VimScrollUnit::row）の半画面と 1 画面は画面行の単位で解く（ADR 0144 の決定 5）。
[[nodiscard]] std::expected<std::optional<VimDisplayNavigation>, VimLayoutNeed>
vim_display_navigation(const VimState &state, const VimEditorView &view, VimAction action);

// 縦の位置合わせ（zt zz zb z<CR> z. z-）の新しい画面の先頭（ADR 0146 の決定 5）。caret は
// 回数で選んで最初の非空白へ動いた後のキャレット。文字組みが無効なら 1 論理行 = 1 画面行の
// 算術、wrap on の text 形は論理行の単位で行の画面行数を問い（Vim の scroll_cursor_bot と
// scroll_cursor_halfway）、格子は画面行の単位。先頭は文書の先頭で止め、上限は収めない。
// wrap on でキャレットの行が画面より高ければ、先頭はその行で、今の first_row をキャレットの画面行が
// 見える範囲に収める。足りない文字組みの値は VimLayoutNeed。
[[nodiscard]] std::expected<VisualPosition, VimLayoutNeed>
vim_aligned_top(const VimEditorView &view, Offset caret, VimScreenPlace place);

// z+ の回数なしの行（ADR 0146 の決定 9）。窓の 1 つ下の行（Vim の botline・画面に全部入っている
// 最後の行の次）で、文書の末尾を越えれば最終行。文字組みが無効なら first_visible + visible_lines、
// wrap on の text 形は window_of の入らなくなった最初の行、格子は画面の最後の画面行の次の画面行の
// 行（同じ行の続きなら同じ行）。足りない文字組みの値は VimLayoutNeed。
[[nodiscard]] std::expected<LineNumber, VimLayoutNeed>
vim_line_below_window(const VimEditorView &view);

// z^ の回数なしの行（決定 9）。窓の 1 つ上の行で、1 行目が先頭なら 1 行目。格子で先頭の画面行が
// 行の途中（first_row > 0）なら、1 つ上の画面行は同じ行にある。
[[nodiscard]] LineNumber vim_line_above_window(const VimEditorView &view) noexcept;
} // namespace nenenib::core
