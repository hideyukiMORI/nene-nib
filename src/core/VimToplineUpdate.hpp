#pragma once

#include "LineNumber.hpp"
#include "VimToplineWindow.hpp"

#include <cstddef>

namespace nenenib::core
{
// Vim モードの追従の 1 本（Vim 9.1 の move.c の update_topline を折り畳み・diff・smoothscroll・
// scrolljump なし（1）でなぞる・#1178・ADR 0019 の追補）。上側は check_top_offset → n と
// halfheight → scroll_cursor_halfway / scroll_cursor_top、下側は check_botline →
// scroll_cursor_bot / scroll_cursor_halfway。窓の先頭は論理行で、行の高さは window.rows の
// plines。キャレットは動かさない（Vim の cursor_correct
// は画面移動の着地だけ・vim_cursor_corrected）。
[[nodiscard]] LineNumber vim_topline_updated(const VimToplineWindow &window, LineNumber caret,
                                             std::size_t line_count) noexcept;

// 同じ update_topline が窓を動かさないか（check_top_offset と check_botline がどちらも要らない）。
// 読むのは窓の先頭から窓の画面行の分の行の高さだけなので、application はこれが偽のときだけ
// キャレットの周りの行を文字組みしてから vim_topline_updated を呼ぶ。
[[nodiscard]] bool vim_topline_kept(const VimToplineWindow &window, LineNumber caret,
                                    std::size_t line_count) noexcept;
} // namespace nenenib::core
