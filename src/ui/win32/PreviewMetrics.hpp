#pragma once

#include "PreviewBlock.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewTaskBox.hpp"
#include "PreviewTextStyle.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

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
// 等幅で描く要素（コードブロックと HTML ブロック）か。
[[nodiscard]] bool preview_monospaced(core::PreviewBlockKind kind) noexcept;
// 要素の layout の幅。枠の幅から字下げ indent（PreviewBlockTops が決める）と右の余白、
// コードブロックは面の内側の余白を引く。
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
// 枠の上の余白。
[[nodiscard]] float preview_top_margin(std::uint32_t dpi) noexcept;
// 前の要素と次の要素の間（段落の後は行高の半分・見出しの上は行高・水平線の上下は行高）。
// same_list は同じリストの続く項目（間は 0）で、種類の違うリストの境は段落と同じ半行。
[[nodiscard]] float preview_gap(const core::PreviewBlock &previous, const core::PreviewBlock &next,
                                const PreviewTextStyle &style, bool same_list) noexcept;
// 要素の縦の大きさ。text_height は layout の GetMetrics().height（水平線は使わない）。
[[nodiscard]] float preview_extent(core::PreviewBlockKind kind, float text_height,
                                   std::uint32_t dpi) noexcept;
// layout に渡す文字（コードブロックと HTML ブロックの末尾の改行を 1 つ落とす）。
[[nodiscard]] std::string_view preview_shown_text(const core::PreviewBlock &block) noexcept;
// 要素の文字の中のバイト位置 → layout の UTF-16 の位置（core の変換 utf16_length の 1 本で数える・
// 装飾の範囲の写しは PreviewTextLayouts と PreviewPainter がこれを読む）。
[[nodiscard]] std::uint32_t preview_units(std::string_view shown, std::size_t byte);
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
