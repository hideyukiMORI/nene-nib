#pragma once

#include "LineNumber.hpp"
#include "VimCursorWindow.hpp"

#include <cstddef>

namespace nenenib::core
{
// 画面移動の後のキャレットの行を余白の内側へ寄せる 1 本（Vim 9.1 の move.c の cursor_correct・
// 折り畳みは無い・#1099・ADR 0019 の追補）。窓の先頭が 1 行目なら上の余白は 0
// で下は画面の半分まで、 本文の末尾まで見えていれば下の余白は 0 で上は (画面 - 1) / 2
// まで。余白が窓に収まらなければ 窓の両端から 1 行ずつ詰めて残った行に着く。窓は動かさない（追従は
// application の 1 か所）。 H M L（オペレータの無いとき）と Ctrl-f / Ctrl-b / Ctrl-d / Ctrl-u
// の着地が呼ぶ。
[[nodiscard]] LineNumber vim_cursor_corrected(const VimCursorWindow &window, LineNumber caret,
                                              std::size_t line_count) noexcept;
} // namespace nenenib::core
