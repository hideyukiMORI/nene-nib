#pragma once

#include "PreviewBlock.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewTableAlign.hpp"
#include "PreviewTaskBox.hpp"
#include "PreviewTextStyle.hpp"

#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
// プレビューの寸法の唯一の表（ADR 0151 の決定 17・採用案 D62 の §2・§3）。DIP の定数を描画先の
// 物理画素に写す（描画先の DPI は core::reference_dpi で、座標は本文と同じ物理画素）。
// PreviewBlockTops（高さの部分和）と PreviewPainter（描く位置）が同じ関数を読む。

// DIP → 物理画素（小数のまま）。
[[nodiscard]] float preview_pixels(float dips, std::uint32_t dpi) noexcept;
// 本文の字の大きさ（物理画素）と行高（本文と同じ比 1.6・BodyLayout と同じ丸め）。
[[nodiscard]] float preview_font_pixels(const PreviewTextStyle &style) noexcept;
[[nodiscard]] float preview_line_pixels(const PreviewTextStyle &style) noexcept;
// 見出しの倍率（h1 1.6・h2 1.35・h3 1.15・ほかは 1）。
[[nodiscard]] float preview_heading_scale(std::uint8_t level) noexcept;
// 等幅で描く要素（コードブロック・HTML ブロック・front matter）か。
[[nodiscard]] bool preview_monospaced(core::PreviewBlockKind kind) noexcept;
// 囲みのコードの面（panel・内側の余白・折らずに面で切る）に描く要素
// （コードブロックと front matter・#1840）か。
[[nodiscard]] bool preview_paneled(core::PreviewBlockKind kind) noexcept;
// 要素の layout の幅。枠の幅から字下げ indent（PreviewBlockTops が決める）と右の余白、
// 面に描く要素（preview_paneled）は面の内側の余白を引く。
[[nodiscard]] float preview_text_width(const core::PreviewBlock &block, float frame_width,
                                       float indent, std::uint32_t dpi) noexcept;
// 左右の余白（16 DIP）・引用の深さ depth（0 から）の左の線の左端・箇条書きの 1 段の字下げの
// 最小（記号の列はこれと「最大の記号の幅＋記号と文字の間」の大きい方）・記号と文字の間（8 DIP）。
[[nodiscard]] float preview_side_margin(std::uint32_t dpi) noexcept;
[[nodiscard]] float preview_quote_left(std::uint8_t depth, std::uint32_t dpi) noexcept;
[[nodiscard]] float preview_list_indent(std::uint32_t dpi) noexcept;
[[nodiscard]] float preview_marker_gap(std::uint32_t dpi) noexcept;
// コードブロックの面の内側の余白。
[[nodiscard]] float preview_code_padding(std::uint32_t dpi) noexcept;
// 面に描く要素の文字の上の余白（上の余白を持たない切れ（middle・last・#1863）は 0・面に描かない
// 要素も 0）と下の余白（下の余白を持たない切れ（first・middle）は 0）。
[[nodiscard]] float preview_panel_top_inset(const core::PreviewBlock &block,
                                            std::uint32_t dpi) noexcept;
[[nodiscard]] float preview_panel_bottom_inset(const core::PreviewBlock &block,
                                               std::uint32_t dpi) noexcept;
// next が previous と同じ囲みのコードの続く切れか（#1863・間の空きは 0 で面が 1 枚に見える）。
[[nodiscard]] bool preview_code_continues(const core::PreviewBlock &previous,
                                          const core::PreviewBlock &next) noexcept;
// 枠の上の余白。
[[nodiscard]] float preview_top_margin(std::uint32_t dpi) noexcept;
// 表（採用案 D103・#1848）: 升の左右の余白 14 DIP・上下の余白 7 DIP・見出しの下の罫 2 DIP・行の間の
// 罫 1 DIP。行の高さは行高に上下の余白を足した一定の値で、升の中では折り返さない。
[[nodiscard]] float preview_table_padding_x(std::uint32_t dpi) noexcept;
[[nodiscard]] float preview_table_padding_y(std::uint32_t dpi) noexcept;
[[nodiscard]] float preview_table_rule(bool header, std::uint32_t dpi) noexcept;
// 行の原点（字下げの位置）から column 番目の升の文字の箱の左端まで（前の列の幅と左右の余白の和
// ＋ 自分の左の余白）。columns は列ごとの中身の幅（物理画素）。
[[nodiscard]] float preview_table_cell_left(std::span<const float> columns, std::size_t column,
                                            std::uint32_t dpi) noexcept;
// 表の幅（列の幅と左右の余白の和）。
[[nodiscard]] float preview_table_width(std::span<const float> columns, std::uint32_t dpi) noexcept;
// 列の幅 column_width の中で、幅 cell_width の升の文字を揃えるために箱の左端から足す量。
[[nodiscard]] float preview_table_aligned(float column_width, float cell_width,
                                          core::PreviewTableAlign align) noexcept;
// alert の左の線の太さ（3 DIP・ふつうの引用は 2 DIP）。
[[nodiscard]] float preview_quote_rule(bool alert, std::uint32_t dpi) noexcept;
// 前の要素と次の要素の間（段落の後は行高の半分・見出しの上は行高・水平線の上下は行高）。
// 囲みのコードの続く切れの間は 0（#1863）。same_list は同じリストの続く項目（間は 0）で、
// 種類の違うリストの境は段落と同じ半行。
[[nodiscard]] float preview_gap(const core::PreviewBlock &previous, const core::PreviewBlock &next,
                                const PreviewTextStyle &style, bool same_list) noexcept;
// 要素の縦の大きさ。text_height は layout の GetMetrics().height（水平線は使わない）。面に描く
// 要素は文字の上下の余白（preview_panel_top_inset・preview_panel_bottom_inset）を足す。
[[nodiscard]] float preview_extent(const core::PreviewBlock &block, float text_height,
                                   std::uint32_t dpi) noexcept;
// 要素の文字の中のバイト位置 → layout の UTF-16 の位置（core の変換 utf16_length の 1 本で数える・
// 装飾の範囲の写しは PreviewTextLayouts と PreviewPainter がこれを読む）。
[[nodiscard]] std::uint32_t preview_units(std::string_view shown, std::size_t byte);
// 要素の文字の中のバイトの範囲（始まり・終わり）。
using PreviewByteRange = std::pair<std::size_t, std::size_t>;
// バイトの範囲の列 → layout の UTF-16 の範囲の列（同じ順・終わりが始まり以前なら長さ 0）。
// 端の値は preview_units と同じで、端を並べて前から 1 回だけ数える（範囲ごとに先頭から
// 数え直さない・#1485）。
[[nodiscard]] std::vector<DWRITE_TEXT_RANGE>
preview_unit_ranges(std::string_view shown, std::span<const PreviewByteRange> ranges);
// layout の UTF-16 の位置 → 要素の文字の中のバイト位置（preview_units の逆・hit test が読む）。
// code point の途中（代理対の後ろ半分）に落ちた位置はその code point の頭へ寄せ、文字の外は末尾。
[[nodiscard]] std::size_t preview_byte_of_units(std::string_view shown, std::uint32_t units);

// layout の UTF-16 の範囲 [first, first + length) を囲む矩形（原点 origin
// から・折り返した範囲は全部 の行を囲む）。測れなければ nullopt（リンクの札の置き場・#1869 の段
// L2）。
[[nodiscard]] std::optional<D2D1_RECT_F> preview_text_range_box(IDWriteTextLayout &layout,
                                                                std::uint32_t first,
                                                                std::uint32_t length,
                                                                D2D1_POINT_2F origin);
// 箇条書きの記号（深さ 1 は •・2 は ◦・3 以上は ▪）。番号はそのまま。
[[nodiscard]] std::string_view preview_marker(const core::PreviewBlock &block) noexcept;
// 記号を描く layout の要素（本文の字体の段落）と、その layout の幅（折り返さないだけ広い）。
// 記号の layout も PreviewTextLayouts の保持に入る。
[[nodiscard]] core::PreviewBlock preview_marker_block(const core::PreviewBlock &item);
[[nodiscard]] float preview_marker_layout_width() noexcept;
// タスクの項目のチェックボックスの一辺（本文の字の大きさの 9 割・物理画素の整数・ADR 0155 の
// 追補 23）。記号の列はこれと記号と文字の間を足した幅より狭くならない。
[[nodiscard]] float preview_task_side(const PreviewTextStyle &style) noexcept;
// 文字の左端 indent・要素の上端 top（どちらも枠の左上から）の項目のチェックボックス。正方形は
// 記号と同じく文字の左端から記号と文字の間だけ空けて右に揃え、1 行目の縦の中央に置く。
[[nodiscard]] PreviewTaskBox preview_task_box(float indent, float top,
                                              const PreviewTextStyle &style) noexcept;
} // namespace nenenib::ui::win32
