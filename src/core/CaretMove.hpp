#pragma once

#include "CaretMoveRequest.hpp"
#include "Offset.hpp"
#include "TextBuffer.hpp"

#include <cstddef>

namespace nenenib::core
{
// 移動後のキャレット位置を返す純関数（ARC-007）。本文は変わらない。
// page_lines は PgUp / PgDn が動く行数で、見えている行数を application が渡す。
[[nodiscard]] Offset moved_caret(const TextBuffer &text, Offset caret, CaretMoveRequest request);

// 行と桁で指したキャレットの置き場（クリックと意図 PlaceCaret・#971）。行を 1〜行数へ、桁を行の
// 内容の中（CRLF の '\r' の前）へ切り詰め、コードポイントの境目に着く。unit が ordinary なら、
// 1 文字の途中（結合文字の列の中）は moved_caret と同じくその文字の先頭へ寄せる（ADR 0065 の
// 決定 4）。後ろへは寄せない。
[[nodiscard]] Offset placed_caret(const TextBuffer &text, const TextPosition &position,
                                  CaretUnit unit);
} // namespace nenenib::core
