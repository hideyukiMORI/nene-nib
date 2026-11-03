#pragma once

#include "LineNumber.hpp"
#include "VimRowsNeeded.hpp"
#include "VimToplineWindow.hpp"

#include <cstddef>
#include <expected>

namespace nenenib::core
{
// Vim モードの追従の 1 本（Vim 9.1 の move.c の update_topline を折り畳み・diff・smoothscroll・
// scrolljump なし（1）でなぞる・#1178・ADR 0019 の追補）。上側は check_top_offset → n と
// halfheight → scroll_cursor_halfway / scroll_cursor_top、下側は check_botline →
// scroll_cursor_bot / scroll_cursor_halfway。窓の先頭は論理行で、行の高さは window.rows の
// plines。読んだ行の高さが載っていなければ（window.asks が真の道）その行を VimRowsNeeded で返す。
// 読むのは Vim の update_topline が読む行だけ（窓が動かなければ窓に描く行とキャレットの下の余白の
// 行・動けば上と下へ足す行）。キャレットは動かさない（Vim の cursor_correct は画面移動の着地だけ・
// vim_cursor_corrected）。
[[nodiscard]] std::expected<LineNumber, VimRowsNeeded>
vim_topline_updated(const VimToplineWindow &window, LineNumber caret,
                    std::size_t line_count) noexcept;
} // namespace nenenib::core
