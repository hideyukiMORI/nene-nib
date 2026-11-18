#pragma once

#include "LayoutRect.hpp"
#include "PreviewBlock.hpp"
#include "PreviewBrushes.hpp"
#include "PreviewInk.hpp"
#include "PreviewInlineImage.hpp"
#include "PreviewInlineImages.hpp"
#include "PreviewMetrics.hpp"
#include "PreviewScene.hpp"
#include "PreviewSelectionText.hpp"
#include "PreviewStyle.hpp"
#include "PreviewTableCell.hpp"
#include "PreviewTaskBox.hpp"
#include "PreviewTextLayout.hpp"
#include "PreviewTextLayouts.hpp"
#include "RenderFailure.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
// Markdown プレビューを描く経路（ADR 0151 の決定 17・採用案 D62 の案 B）。上から高さを足した
// 部分和（PreviewBlockTops・錨の要素から枠の高さまで）で見えている要素だけを DrawTextLayout
// で描き、面と線は FillRectangle / FillRoundedRectangle。Target は製品では Direct2D の描画先
// （ID2D1RenderTarget）で、tests/ui は呼び出しを数える替え玉、tests/preview は WIC の bitmap
// の描画先を渡す。色は役割（PreviewInk）→ PreviewBrushes の brush だけで、色の値を書かない。
// 読めた画像（ADR 0155 の追補 62）は layout の箱の場所に、文字を描いた後で DrawBitmap で描く。
template <typename Target> class PreviewPainter final
{
  public:
    PreviewPainter(Target &target, PreviewTextLayouts &layouts, const PreviewBrushes &brushes,
                   std::optional<RenderFailure> &failure)
        : target_(target), layouts_(layouts), brushes_(brushes), failure_(failure)
    {
    }
    // 枠を地の色で塗り、枠の中で見えている要素を描く。失敗は failure に置いて止まる。
    void paint(const PreviewScene &scene)
    {
        const auto frame = rect_of(scene.frame);
        target_.FillRectangle(frame, brushes_.of(PreviewInk::background));
        layouts_.bind(brushes_);
        target_.PushAxisAlignedClip(frame, D2D1_ANTIALIAS_MODE_ALIASED);
        const auto height = static_cast<float>(core::height_of(scene.frame));
        for (auto index = scene.tops.first_visible(scene.scroll);
             index < scene.tops.size() && scene.tops.top(index) - scene.scroll < height &&
             !failure_.has_value();
             ++index)
        {
            draw_block(scene, index);
        }
        target_.PopAxisAlignedClip();
    }

  private:
    static constexpr float inline_padding_dips = 4.0F;
    static constexpr float inline_radius_dips = 3.0F;
    static constexpr float code_radius_dips = 5.0F;
    // チェックボックスの枠と線の太さ（DIP）と、チェックの 3 点（一辺に対する比）。
    static constexpr float task_stroke_dips = 1.0F;
    static constexpr float check_stroke_dips = 2.0F;
    static constexpr float check_start_x = 0.22F;
    static constexpr float check_start_y = 0.52F;
    static constexpr float check_low_x = 0.42F;
    static constexpr float check_low_y = 0.72F;
    static constexpr float check_end_x = 0.78F;
    static constexpr float check_end_y = 0.28F;
    // 行内コードの面は行高（本文の 1.6 倍）の上下を 1 割ずつ空ける。
    static constexpr float inline_inset_ratio = 0.1F;

    [[nodiscard]] static D2D1_RECT_F rect_of(const core::LayoutRect &area) noexcept
    {
        return D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                           static_cast<float>(area.right), static_cast<float>(area.bottom));
    }

    // 要素の文字の色（見出しは keyword・コードは string・HTML と水平線と front matter は comment・
    // 引用の中の段落と項目と表の行は comment（alert の引用の中は foreground・#1848）・
    // ほかは foreground）。
    [[nodiscard]] static PreviewInk ink_of(const core::PreviewBlock &block) noexcept
    {
        switch (block.kind)
        {
        case core::PreviewBlockKind::heading:
            return PreviewInk::keyword;
        case core::PreviewBlockKind::code:
            return PreviewInk::string;
        case core::PreviewBlockKind::html:
        case core::PreviewBlockKind::rule:
        case core::PreviewBlockKind::front_matter:
            return PreviewInk::comment;
        case core::PreviewBlockKind::paragraph:
        case core::PreviewBlockKind::list_item:
        case core::PreviewBlockKind::table_row:
            return block.quote_depth > 0 && !alerted(block) ? PreviewInk::comment
                                                            : PreviewInk::foreground;
        }
        std::unreachable();
    }

    // alert の引用の中の要素で、alert の線の深さ（いちばん外）の中にある（入れ子の引用の中は
    // ふつうの引用の色）。
    [[nodiscard]] static bool alerted(const core::PreviewBlock &block) noexcept
    {
        return block.alert.has_value() && block.quote_depth == 1;
    }

    void draw_block(const PreviewScene &scene, std::size_t index)
    {
        const auto &block = scene.document.blocks.at(index);
        const auto origin = D2D1::Point2F(
            static_cast<float>(scene.frame.left) + scene.tops.indent(index),
            static_cast<float>(scene.frame.top) + scene.tops.top(index) - scene.scroll);
        draw_quote_rules(scene, index);
        switch (block.kind)
        {
        case core::PreviewBlockKind::rule:
            draw_rule(scene, origin.y, scene.tops.bottom(index) - scene.tops.top(index));
            return;
        case core::PreviewBlockKind::code:
        case core::PreviewBlockKind::front_matter:
            draw_code(scene, index, origin);
            return;
        case core::PreviewBlockKind::list_item:
            draw_item_mark(scene, index, origin);
            draw_text(scene, index, origin);
            return;
        case core::PreviewBlockKind::paragraph:
        case core::PreviewBlockKind::heading:
        case core::PreviewBlockKind::html:
            draw_text(scene, index, origin);
            return;
        case core::PreviewBlockKind::table_row:
            draw_table_row(scene, index, origin);
            return;
        }
    }

    // 引用の深さごとの左の線（comment・2 DIP）。前の要素が同じ深さの引用なら間も塗ってつなぐ。
    // alert の引用のいちばん外の線は 3 DIP で種類の色（#1848・D103）。
    void draw_quote_rules(const PreviewScene &scene, std::size_t index)
    {
        const auto &block = scene.document.blocks.at(index);
        const auto dpi = scene.style.dpi;
        const auto offset = static_cast<float>(scene.frame.top) - scene.scroll;
        for (std::uint8_t depth = 0; depth < block.quote_depth; ++depth)
        {
            // 測った範囲の最初の要素（錨）は枠の上端から塗るので、前の要素とはつながない。
            // alert の種類が違う前の要素（隣り合う別の引用）とは、いちばん外の線をつながない。
            const bool joined =
                index > scene.tops.first() &&
                scene.document.blocks.at(index - 1).quote_depth > depth &&
                (depth > 0 || scene.document.blocks.at(index - 1).alert == block.alert);
            const auto left = static_cast<float>(scene.frame.left) + preview_quote_left(depth, dpi);
            const auto top =
                offset + (joined ? scene.tops.bottom(index - 1) : scene.tops.top(index));
            const bool alert = depth == 0 && block.alert.has_value();
            target_.FillRectangle(
                D2D1::RectF(left, top, left + preview_quote_rule(alert, dpi),
                            offset + scene.tops.bottom(index)),
                brushes_.of(alert ? preview_alert_ink(block.alert.value()) : PreviewInk::comment));
        }
    }

    // 表の 1 行（#1848・採用案 D103 の案 B）。升ごとに折り返さない文字組みを列の揃えで置き、
    // 見出しの行の下に panel_border の 2 DIP、続く行の上に 1 DIP の罫を引く（外枠と縦の罫は
    // 引かない）。枠より広い表は右を枠の余白で切る。列の幅は表ごとに 1 回だけ測った値を読む。
    void draw_table_row(const PreviewScene &scene, std::size_t index, D2D1_POINT_2F origin)
    {
        const auto &block = scene.document.blocks.at(index);
        const auto columns = layouts_.table_columns(scene.document, index, scene.style);
        if (!columns || !block.table.has_value())
        {
            failure_ = columns ? RenderFailure::directwrite : columns.error();
            return;
        }
        const auto dpi = scene.style.dpi;
        const auto bottom = origin.y + scene.tops.bottom(index) - scene.tops.top(index);
        const auto right = static_cast<float>(scene.frame.right) - preview_side_margin(dpi);
        const auto band = D2D1::RectF(origin.x, origin.y, std::max(right, origin.x), bottom);
        target_.PushAxisAlignedClip(band, D2D1_ANTIALIAS_MODE_ALIASED);
        draw_table_rules(scene, index, band, *columns.value());
        for (std::size_t column = 0; column < block.table.value().cells.size() &&
                                     column < columns.value()->size() && !failure_.has_value();
             ++column)
        {
            const auto left = origin.x + preview_table_cell_left(*columns.value(), column, dpi);
            if (left >= right)
            {
                break;
            }
            draw_cell(scene, index, column,
                      D2D1::RectF(left, origin.y, left + columns.value()->at(column), bottom));
        }
        target_.PopAxisAlignedClip();
    }

    // 見出しの行は下に 2 DIP、見出しでない行の前の行が見出しでなければ上に 1 DIP。
    void draw_table_rules(const PreviewScene &scene, std::size_t index, D2D1_RECT_F band,
                          const std::vector<float> &columns)
    {
        const auto &row = scene.document.blocks.at(index).table;
        if (!row.has_value())
        {
            return;
        }
        const auto &table = row.value();
        const auto dpi = scene.style.dpi;
        const auto right = std::min(band.right, band.left + preview_table_width(columns, dpi));
        if (table.header)
        {
            const auto thick = preview_table_rule(true, dpi);
            target_.FillRectangle(D2D1::RectF(band.left, band.bottom - thick, right, band.bottom),
                                  brushes_.of(PreviewInk::panel_border));
            return;
        }
        const auto &previous = scene.document.blocks.at(index - (table.row > 0 ? 1U : 0U));
        if (table.row > 0 && previous.table.has_value() && !previous.table.value().header)
        {
            target_.FillRectangle(
                D2D1::RectF(band.left, band.top, right, band.top + preview_table_rule(false, dpi)),
                brushes_.of(PreviewInk::panel_border));
        }
    }

    // 1 つの升。box は列の中身の箱（左端は列の左の余白の後・幅は列の中身の幅・縦は行の帯）。
    void draw_cell(const PreviewScene &scene, std::size_t index, std::size_t column,
                   D2D1_RECT_F box)
    {
        const auto &block = scene.document.blocks.at(index);
        const auto held = layouts_.paintable(core::preview_table_cell(block, column),
                                             preview_marker_layout_width(), scene.style, {});
        DWRITE_TEXT_METRICS metrics{};
        if (!held || FAILED(held.value()->layout->GetMetrics(&metrics)) || !block.table.has_value())
        {
            failure_ = held ? RenderFailure::directwrite : held.error();
            return;
        }
        const auto &entry = *held.value();
        const auto origin =
            D2D1::Point2F(box.left + preview_table_aligned(box.right - box.left, metrics.width,
                                                           block.table.value().aligns.at(column)),
                          box.top + preview_table_padding_y(scene.style.dpi));
        fill_inline_code(scene, entry, origin);
        const auto selected = cell_selection(scene, index, column);
        if (selected.has_value())
        {
            fill_runs(*entry.layout.Get(), origin, selected.value().first, selected.value().second);
        }
        target_.DrawTextLayout(origin, entry.layout.Get(), brushes_.of(ink_of(block)),
                               D2D1_DRAW_TEXT_OPTIONS_NONE);
    }

    // 全面プレビューの選択のうち、升の範囲に掛かる所の升の文字組みの UTF-16 の範囲（行の文字の
    // 中の選択を升の中へ写す・掛からなければ nullopt）。
    [[nodiscard]] static std::optional<std::pair<std::uint32_t, std::uint32_t>>
    cell_selection(const PreviewScene &scene, std::size_t index, std::size_t column)
    {
        const auto bytes =
            scene.selection.has_value()
                ? core::preview_selected_bytes(scene.document, scene.selection.value(), index)
                : std::nullopt;
        if (!bytes.has_value())
        {
            return std::nullopt;
        }
        const auto &block = scene.document.blocks.at(index);
        const auto [first, second] = core::preview_table_cell_range(block, column);
        const auto begin = std::clamp(bytes.value().first, first, second) - first;
        const auto end = std::clamp(bytes.value().second, first, second) - first;
        if (end <= begin)
        {
            return std::nullopt;
        }
        const auto shown = std::string_view(block.text).substr(first, second - first);
        return std::pair{preview_units(shown, begin), preview_units(shown, end)};
    }

    void fill_runs(IDWriteTextLayout &layout, D2D1_POINT_2F origin, std::uint32_t begin,
                   std::uint32_t end)
    {
        const auto runs =
            layouts_.hit_runs(layout, DWRITE_TEXT_RANGE{begin, end > begin ? end - begin : 0});
        if (!runs)
        {
            failure_ = runs.error();
            return;
        }
        for (const auto &run : runs.value())
        {
            const auto left = origin.x + run.left;
            const auto top = origin.y + run.top;
            target_.FillRectangle(D2D1::RectF(left, top, left + run.width, top + run.height),
                                  brushes_.of(PreviewInk::selection));
        }
    }

    void draw_rule(const PreviewScene &scene, float top, float extent)
    {
        const auto margin = preview_side_margin(scene.style.dpi);
        target_.FillRectangle(D2D1::RectF(static_cast<float>(scene.frame.left) + margin, top,
                                          static_cast<float>(scene.frame.right) - margin,
                                          top + extent),
                              brushes_.of(PreviewInk::comment));
    }

    // コードブロックと front matter: panel の面（角丸 5）の内側の
    // 余白 12 に折らずに描き、面の外は切る。
    void draw_code(const PreviewScene &scene, std::size_t index, D2D1_POINT_2F origin)
    {
        const auto dpi = scene.style.dpi;
        const auto extent = scene.tops.bottom(index) - scene.tops.top(index);
        const auto radius = preview_pixels(code_radius_dips, dpi);
        const auto panel = D2D1::RectF(
            origin.x, origin.y, static_cast<float>(scene.frame.right) - preview_side_margin(dpi),
            origin.y + extent);
        target_.FillRoundedRectangle(D2D1::RoundedRect(panel, radius, radius),
                                     brushes_.of(PreviewInk::panel));
        target_.PushAxisAlignedClip(panel, D2D1_ANTIALIAS_MODE_ALIASED);
        const auto padding = preview_code_padding(dpi);
        draw_text(scene, index, D2D1::Point2F(origin.x + padding, origin.y + padding));
        target_.PopAxisAlignedClip();
    }

    // 項目の記号と番号（operators）。文字の左端から 8 DIP 空けて右に揃える。記号の layout は
    // 折り返さない幅で作り、記号の列の幅は PreviewBlockTops が測る範囲の同じリストの最大の記号で
    // 決める。
    void draw_marker(const PreviewScene &scene, const core::PreviewBlock &block,
                     D2D1_POINT_2F origin)
    {
        const auto marker = preview_marker(block);
        if (marker.empty())
        {
            return;
        }
        const auto layout =
            layouts_.get(preview_marker_block(block), preview_marker_layout_width(), scene.style);
        DWRITE_TEXT_METRICS metrics{};
        if (!layout || FAILED(layout.value()->GetMetrics(&metrics)))
        {
            failure_ = RenderFailure::directwrite;
            return;
        }
        const auto left = origin.x - preview_marker_gap(scene.style.dpi) -
                          metrics.widthIncludingTrailingWhitespace;
        target_.DrawTextLayout(D2D1::Point2F(left, origin.y), layout.value().Get(),
                               brushes_.of(PreviewInk::operators), D2D1_DRAW_TEXT_OPTIONS_NONE);
    }

    // 項目の頭の印。タスクの項目はチェックボックス、ほかは記号と番号（ADR 0155 の追補 23）。
    void draw_item_mark(const PreviewScene &scene, std::size_t index, D2D1_POINT_2F origin)
    {
        const auto &block = scene.document.blocks.at(index);
        const auto &box = scene.tops.task_box(index);
        if (!box.has_value() || !block.task.has_value())
        {
            // 箱のある項目は 1 行目の baseline が下がるので、記号もその分だけ下げる（追補 68）。
            const auto width =
                preview_text_width(block, static_cast<float>(core::width_of(scene.frame)),
                                   scene.tops.indent(index), scene.style.dpi);
            const auto shift = layouts_.first_line_shift(
                block, width, scene.style,
                preview_inline_images(scene.document, scene.images, index,
                                      preview_dips_of(width, scene.style.dpi)));
            if (!shift)
            {
                failure_ = shift.error();
                return;
            }
            draw_marker(scene, block, D2D1::Point2F(origin.x, origin.y + shift.value()));
            return;
        }
        draw_task(scene, box.value(), block.task.value().checked);
    }

    // タスクの項目のチェックボックス（ADR 0155 の追補 23）。字体に依らない Direct2D の図形で、枠は
    // muted の 1 DIP、チェックは accent の 2 本の線。「[ ]」の文字は描かない。
    void draw_task(const PreviewScene &scene, const PreviewTaskBox &box, bool checked)
    {
        const auto dpi = scene.style.dpi;
        const auto left = static_cast<float>(scene.frame.left + box.square.left);
        const auto top = static_cast<float>(scene.frame.top + box.square.top) - scene.scroll;
        const auto side = static_cast<float>(core::width_of(box.square));
        // 枠の線は整数の画素の太さ（1 DIP を切り捨て・最小 1）にし、線の中心を太さの半分だけ内へ
        // 寄せて画素の境に合わせ、滲ませない。
        const auto stroke = std::max(1.0F, std::floor(preview_pixels(task_stroke_dips, dpi)));
        const auto half = stroke / 2.0F;
        target_.DrawRectangle(
            D2D1::RectF(left + half, top + half, left + side - half, top + side - half),
            brushes_.of(PreviewInk::muted), stroke);
        if (!checked)
        {
            return;
        }
        const auto mark = preview_pixels(check_stroke_dips, dpi);
        const auto low = D2D1::Point2F(left + side * check_low_x, top + side * check_low_y);
        target_.DrawLine(D2D1::Point2F(left + side * check_start_x, top + side * check_start_y),
                         low, brushes_.of(PreviewInk::accent), mark);
        target_.DrawLine(low, D2D1::Point2F(left + side * check_end_x, top + side * check_end_y),
                         brushes_.of(PreviewInk::accent), mark);
    }

    // 保持した layout は描くときに書式を変えず、位置も問わない（#1477・ADR 0155 の追補）。色と
    // 行内コードの面の矩形と画像の箱の左上は保持の entry が 1 回だけ置いて持ち、ここは原点を足して
    // 描くだけ。
    void draw_text(const PreviewScene &scene, std::size_t index, D2D1_POINT_2F origin)
    {
        const auto &block = scene.document.blocks.at(index);
        const auto width =
            preview_text_width(block, static_cast<float>(core::width_of(scene.frame)),
                               scene.tops.indent(index), scene.style.dpi);
        const auto inlines = preview_inline_images(scene.document, scene.images, index,
                                                   preview_dips_of(width, scene.style.dpi));
        const auto held = layouts_.paintable(block, width, scene.style, inlines);
        if (!held)
        {
            failure_ = held.error();
            return;
        }
        const auto &entry = *held.value();
        fill_inline_code(scene, entry, origin);
        fill_selection(scene, index, *entry.layout.Get(), origin);
        target_.DrawTextLayout(origin, entry.layout.Get(), brushes_.of(ink_of(block)),
                               D2D1_DRAW_TEXT_OPTIONS_NONE);
        for (std::size_t at = 0; at < inlines.size() && at < entry.image_origins.size(); ++at)
        {
            draw_image(scene, inlines.at(at), entry.image_origins.at(at), origin);
        }
    }

    // 読めた画像 1 枚（ADR 0155 の追補 62）。layout の箱の左上を物理画素の整数に揃えて、
    // 箱の大きさに DrawBitmap する（1 画素 = 1 DIP の大きさなら拡大縮小しないので滲まない）。
    // bitmap が無ければ箱は空のまま（PreviewPane が描く前に作るので、無いのは作れないときだけ）。
    void draw_image(const PreviewScene &scene, const PreviewInlineImage &image,
                    const std::optional<D2D1_POINT_2F> &at, D2D1_POINT_2F origin)
    {
        auto *const bitmap =
            scene.bitmaps == nullptr ? nullptr : scene.bitmaps->find(image.pixels.get());
        // 位置を引けない・bitmap が無い画像は描かず、箱は空のまま（ADR 0155 の追補 67・画像の
        // ために frame を失敗にしない）。
        if (!at.has_value() || bitmap == nullptr)
        {
            return;
        }
        const auto left = std::round(origin.x + at.value().x);
        const auto top = std::round(origin.y + at.value().y);
        target_.DrawBitmap(
            bitmap,
            D2D1::RectF(left, top, left + preview_inline_extent(image.box.width, scene.style.dpi),
                        top + preview_inline_extent(image.box.height, scene.style.dpi)),
            1.0F, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    }

    // 全面プレビューの選択の面（selection・ADR 0155 の改訂 18）。文字の下に、選ばれた範囲の画面行
    // ごとの矩形を塗る。範囲は core の preview_selected_bytes の 1 本で決める。選択は描くたびに
    // 変わるので、ここだけが位置を問う（HitTestTextRange は layout を汚さない）。
    void fill_selection(const PreviewScene &scene, std::size_t index, IDWriteTextLayout &layout,
                        D2D1_POINT_2F origin)
    {
        if (!scene.selection.has_value())
        {
            return;
        }
        const auto bytes =
            core::preview_selected_bytes(scene.document, scene.selection.value(), index);
        if (!bytes.has_value())
        {
            return;
        }
        const auto shown = core::preview_shown_text(scene.document.blocks.at(index));
        fill_runs(layout, origin, preview_units(shown, bytes.value().first),
                  preview_units(shown, bytes.value().second));
    }

    // 行内コードの面（panel・角丸 3・左右 4 DIP）。折り返した範囲は画面行ごとの矩形。
    void fill_inline_code(const PreviewScene &scene, const PreviewTextLayout &entry,
                          D2D1_POINT_2F origin)
    {
        const auto dpi = scene.style.dpi;
        const auto padding = preview_pixels(inline_padding_dips, dpi);
        const auto radius = preview_pixels(inline_radius_dips, dpi);
        for (const auto &run : entry.code_runs)
        {
            const auto inset = run.height * inline_inset_ratio;
            const auto left = origin.x + run.left;
            const auto top = origin.y + run.top;
            const auto rect = D2D1::RectF(left - padding, top + inset, left + run.width + padding,
                                          top + run.height - inset);
            target_.FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius),
                                         brushes_.of(PreviewInk::panel));
        }
    }

    Target &target_;
    PreviewTextLayouts &layouts_;
    const PreviewBrushes &brushes_;
    std::optional<RenderFailure> &failure_;
};
} // namespace nenenib::ui::win32
