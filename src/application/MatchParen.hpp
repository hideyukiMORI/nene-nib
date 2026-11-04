#pragma once

#include "ColorSpan.hpp"
#include "EditorState.hpp"
#include "LineView.hpp"
#include "OffsetRange.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace nenenib::application
{
// matchparen（ADR 0164・Issue #999）。キャレットの下の括弧の相手を、
// 見えている行と前後 screen 行の窓で core の matched_bracket に探させる。
// INSERT と通常モード（棒のキャレット）では直前の括弧も見る。相手が見えている行に
// あれば、その行の LineView::bracket に写す。構文の区間（string / comment）は、
// キャレットの括弧が区間の外なら中の括弧を数えず、中なら同じ種類のつながった
// 区間の中だけで探す（決定 3）。lines は操作中の枠の text の表示の行で、格子・
// 全面プレビュー・問いの間は空。変換中（IME）と巨大な行のキャレットでは探さない。
void mark_bracket(const EditorState &state, std::size_t screen, std::vector<LineView> &lines);

// 行の色の区間の桁（code point の序数・昇順で重ならない）を、同じ順の行の中の
// バイト位置の区間へ写す。行は先頭から 1 回だけ歩く（区間の数 × 行の長さにしない）。
[[nodiscard]] std::vector<core::OffsetRange>
color_span_bytes(std::string_view text, std::span<const core::ColorSpan> colors);
} // namespace nenenib::application
