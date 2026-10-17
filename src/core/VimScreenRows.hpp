#pragma once

#include "VimDisplayNavigation.hpp"
#include "VimEditorView.hpp"
#include "VimLayoutNeed.hpp"
#include "VimScrollDirection.hpp"
#include "VimState.hpp"

#include <cstddef>
#include <expected>

namespace nenenib::core
{
// 画面行の単位の画面移動（ADR 0144 の決定 3・5）。画面の先頭（first_visible の行の first_row 番目の
// 画面行）とキャレットの画面行を、画面行の列として同じ数だけ direction へ amount 行まで送る。
// 先頭は本文の最初の画面行から最後の画面行までで止まり、動けなければ何も変えない（失敗にしない）。
// キャレットは送った後の画面の中に収め、行き先の画面行の中で欲しい列の仮想桁に着く（$ の後は
// 画面行の末尾）。足りない行の画面行数と画面行の範囲は VimLayoutNeed で返し、状態は変えない。
[[nodiscard]] std::expected<VimDisplayNavigation, VimLayoutNeed>
vim_row_navigation(const VimState &state, const VimEditorView &view, VimScrollDirection direction,
                   std::size_t amount);
} // namespace nenenib::core
