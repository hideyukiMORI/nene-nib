#pragma once

#include "LineNumber.hpp"
#include "LineWindow.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VirtualColumnIndex.hpp"

#include <cstddef>
#include <utility>

namespace nenenib::application
{
// ADR 0139: text の表示の巨大な行の窓。判定・窓・桁の行き来は本文が持つ仮想桁の索引
// （TextBuffer::virtual_columns・決定 6）から O(塊) で引き、行の全体を写さない。

// 巨大な行（決定 1）なら本文の索引、そうでなければ nullptr。索引を持たない文書は巨大な行を
// 持たない（開く道が巨大な行の文書に索引を持たせる・ADR 0140 の決定 4）。
[[nodiscard]] const core::VirtualColumnIndex *giant_line_index(const core::TextBuffer &text,
                                                               core::LineNumber line);

// 窓の見積もりに使う字幅（物理画素）。測る前（0）は 1 画素。
[[nodiscard]] float window_advance(core::TextViewGeometry geometry) noexcept;

// anchor の周りの窓（決定 3）と、その本文のバイト範囲。窓の x は 0 のまま返し、呼ぶ側が
// window_x で origin からの相対にする。
[[nodiscard]] std::pair<LineWindow, core::OffsetRange>
giant_window_at(const core::VirtualColumnIndex &index, const core::TextBuffer &text,
                core::LineNumber line, core::Offset anchor);

// 仮想桁 column の見積もった x を、origin（キャレットの行の窓の始まりの仮想桁・キャレットが巨大な
// 行に無ければ 0）からの相対で。差を整数で取ってから掛け、大きな値の桁落ちを避ける（決定 5）。
[[nodiscard]] float window_x(std::size_t column, std::size_t origin, float advance) noexcept;

// 巨大な行の中の本文の位置とバイト位置の行き来（序数は索引から O(塊)）。
[[nodiscard]] core::Offset giant_offset_of(const core::VirtualColumnIndex &index,
                                           const core::TextBuffer &text,
                                           core::TextPosition position);
[[nodiscard]] core::TextPosition giant_position_of(const core::VirtualColumnIndex &index,
                                                   const core::TextBuffer &text,
                                                   core::LineNumber line, core::Offset at);
} // namespace nenenib::application
