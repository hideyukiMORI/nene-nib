#pragma once

#include "Diagnostic.hpp"
#include "DiagnosticSeverity.hpp"
#include "Offset.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 診断の間を跳ぶ・札の文・一覧の順の純関数（#1939・ADR 0161 の決定 9 の追補）。どれも文書の値の
// 診断（本文のバイト位置）だけを読み、サーバーへは何も頼まない。

// キャレットが診断の上にあるか。空の区間は始まりの位置だけ、ほかは半開区間（1 行の知らせと札が
// 同じ 1 本を使う）。
[[nodiscard]] bool diagnostic_covers(const Diagnostic &diagnostic, Offset caret) noexcept;

// 一覧と跳びの順（行 → 桁 → 重さ）。始まりの位置の小さい順、同じ位置は重い順、それも同じなら
// 届いた順。結果は diagnostics の中の位置の列。
[[nodiscard]] std::vector<std::size_t> diagnostic_order(std::span<const Diagnostic> diagnostics);

// `]d`（forward）・`[d` を count 回（count は 1 以上・0 は 1
// と同じ）。歩くのは診断の始まりの位置で、 同じ位置の診断は 1
// つに数える（重さで絞らない）。次はキャレットより後ろで最も近い始まり、前は
// キャレットより前で最も近い始まりで、端を越えたら反対の端へ折り返す（Neovim の既定と同じ）。
// 診断が無ければ値なし。
[[nodiscard]] std::optional<Offset> diagnostic_landing(std::span<const Diagnostic> diagnostics,
                                                       Offset caret, bool forward,
                                                       std::size_t count);

// 札の文（`gl`・跳んだ後）。キャレットの上の診断を重い順（同じ重さは diagnostic_order の順）に、
// 文の全部と、出どころと番号の行（`clangd(unused-variable)`・片方だけならその片方）を並べ、診断の
// 間は空の行で区切る。キャレットの上に無ければ空。行の上限は札の側（hover_tip_text）が切る。
[[nodiscard]] std::string diagnostic_tip_text(std::span<const Diagnostic> diagnostics,
                                              Offset caret);

// 一覧の行の重さの印（ステータスバーの件数と同じ E W、続いて I H）。
[[nodiscard]] std::string_view diagnostic_severity_mark(DiagnosticSeverity severity) noexcept;
} // namespace nenenib::core
