#pragma once

#include "CaretMotion.hpp"
#include "MarkdownDocument.hpp"
#include "Offset.hpp"
#include "PreviewBlock.hpp"
#include "PreviewPoint.hpp"
#include "PreviewSelection.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
// プレビューの見えている文字と、全面プレビューの選択の純関数（ADR 0155 の改訂 18）。描く側
// （ui の PreviewTextLayouts・PreviewPainter・hit test）と選択・コピーの側（application）が同じ
// 1 本を読む。位置は PreviewPoint（要素の番号と、要素の見えている文字の中のバイト位置）。

// 要素の見えている文字。layout に渡す文字と同じで、コードブロックと HTML ブロックの末尾の改行を
// 1 つ落とす。水平線は文字を描かない（選択とコピーでは飛ばす）。
[[nodiscard]] std::string_view preview_shown_text(const PreviewBlock &block) noexcept;

// anchor を含む要素（source_begin の upper_bound − 1・anchor より前に要素が無ければ 0）。
[[nodiscard]] std::size_t preview_anchor_block(const MarkdownDocument &document, Offset anchor);

// 選択の 2 つの端（選び始めた端・動く端）と、端から作る選択。
[[nodiscard]] PreviewPoint preview_selection_origin(const PreviewSelection &selection) noexcept;
[[nodiscard]] PreviewPoint preview_selection_head(const PreviewSelection &selection) noexcept;
[[nodiscard]] PreviewSelection preview_selection_of(PreviewPoint origin,
                                                    PreviewPoint head) noexcept;

// 文書の中に切り詰めた点（要素の番号は最後の要素まで・位置は見えている文字の長さまで）。空の
// 文書では {0, 0}。
[[nodiscard]] PreviewPoint preview_point_clamped(const MarkdownDocument &document,
                                                 PreviewPoint point) noexcept;

// 選択を文書の順に並べて切り詰めた [first, last)。空の文書と空の選択は nullopt。
[[nodiscard]] std::optional<std::pair<PreviewPoint, PreviewPoint>>
preview_selection_range(const MarkdownDocument &document,
                        const PreviewSelection &selection) noexcept;

// 要素 block の見えている文字のうち選ばれたバイトの範囲 [begin, end)（選択の面を塗る範囲）。選択の
// 外・範囲が空・水平線は nullopt。
[[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>>
preview_selected_bytes(const MarkdownDocument &document, const PreviewSelection &selection,
                       std::size_t block) noexcept;

// コピーの本文（改訂 18）。選ばれた見えている文字（Markdown の記号を除いた描いた文字・リストの点と
// 番号とチェックボックスは入れない・コードブロックはそのまま）を、要素の境に改行 1 つ（LF）を挟んで
// つなぐ。水平線は飛ばす。空の選択は空。
[[nodiscard]] std::string preview_selection_text(const MarkdownDocument &document,
                                                 const PreviewSelection &selection);

// 全部の要素の選択（Ctrl+A）。空の文書は nullopt。
[[nodiscard]] std::optional<PreviewSelection>
preview_selection_all(const MarkdownDocument &document) noexcept;

// Shift+矢印で動く端の行き先（改訂 18）。左右は 1 文字（vim_character_end / vim_character_start
// の 1 対・要素の端では隣の要素の端へ）、上下は要素の頭と終わり（頭・終わりに居れば前・次の要素
// の頭・終わり）、Home / End は要素の頭と終わり、Ctrl+Home / End は文書の頭と終わり。語と
// PgUp / PgDn は動かない。水平線は飛ばす。
[[nodiscard]] PreviewPoint preview_point_moved(const MarkdownDocument &document, PreviewPoint point,
                                               CaretMotion motion) noexcept;
} // namespace nenenib::core
