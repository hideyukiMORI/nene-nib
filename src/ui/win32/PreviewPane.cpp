#include "PreviewPane.hpp"

#include "MarkdownImage.hpp"
#include "PreviewAnchoredBlock.hpp"
#include "PreviewBlock.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewHitMode.hpp"
#include "PreviewInlineImages.hpp"
#include "PreviewMetrics.hpp"
#include "PreviewPainter.hpp"
#include "PreviewScene.hpp"
#include "PreviewScrollFold.hpp"
#include "PreviewScrollSpot.hpp"
#include "PreviewSelectionText.hpp"
#include "PreviewTableCell.hpp"
#include "PreviewTableMetrics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
// layout の点 point の UTF-16 の位置。character は文字の上（isInside）のときだけその文字の
// 始まり、edge は近い端（後ろ半分なら文字の後ろ）。測れなければ nullopt。
[[nodiscard]] std::optional<std::uint32_t> hit_units(IDWriteTextLayout &layout, D2D1_POINT_2F point,
                                                     PreviewHitMode mode)
{
    BOOL trailing = FALSE;
    BOOL inside = FALSE;
    DWRITE_HIT_TEST_METRICS metrics{};
    if (FAILED(layout.HitTestPoint(point.x, point.y, &trailing, &inside, &metrics)) ||
        (mode == PreviewHitMode::character && inside == FALSE))
    {
        return std::nullopt;
    }
    return metrics.textPosition +
           (mode == PreviewHitMode::edge && trailing != FALSE ? metrics.length : 0U);
}

[[nodiscard]] bool same_viewport(const PreviewViewport &left, const PreviewViewport &right) noexcept
{
    return left.first == right.first && left.width == right.width && left.height == right.height &&
           left.nudge == right.nudge && left.through == right.through;
}

} // namespace

std::expected<const PreviewBlockTops *, RenderFailure>
PreviewPane::remeasure(const std::shared_ptr<const core::MarkdownDocument> &document,
                       const std::shared_ptr<const application::PreviewImages> &images,
                       const PreviewTextStyle &style, const PreviewViewport &viewport)
{
    const bool same_images =
        !measured_images_.owner_before(images) && !images.owner_before(measured_images_);
    if (tops_.has_value() && measured_ == document && same_images && measured_style_.has_value() &&
        measured_style_.value() == style && same_viewport(measured_viewport_, viewport))
    {
        return &tops_.value();
    }
    tops_.reset();
    measured_ = nullptr;
    measured_images_.reset();
    auto tops = PreviewBlockTops::measure(*document, layouts_, style, viewport);
    if (!tops)
    {
        return std::unexpected(tops.error());
    }
    measured_ = document;
    measured_images_ = images;
    measured_style_ = style;
    measured_viewport_ = viewport;
    return &tops_.emplace(std::move(tops).value());
}

std::expected<void, RenderFailure> PreviewPane::prepare(ID2D1RenderTarget &target,
                                                        const core::Theme &theme)
{
    if (FAILED(brushes_.prepare(target, theme)))
    {
        return std::unexpected(RenderFailure::direct2d);
    }
    theme_ = theme.name;
    return {};
}

std::expected<void, RenderFailure> PreviewPane::draw_scene(ID2D1RenderTarget &target,
                                                           const PreviewPictureKey &key,
                                                           const core::LayoutRect &frame)
{
    // 鍵は弱い参照で覚えるので、描く間だけ強い参照にする（frame が持っているので消えていない）。
    const auto images = key.images.lock();
    const PreviewViewport viewport{key.anchor, static_cast<float>(key.width),
                                   static_cast<float>(key.height), images.get(),
                                   preview_pixels(key.nudge, key.style.dpi)};
    const auto measured = remeasure(key.document, images, key.style, viewport);
    if (!measured)
    {
        return std::unexpected(measured.error());
    }
    const auto held = hold_bitmaps(target, *key.document, images.get(), *measured.value());
    if (!held)
    {
        return held;
    }
    ++painted_;
    std::optional<RenderFailure> failure;
    PreviewPainter<ID2D1RenderTarget> painter(target, layouts_, brushes_, failure);
    painter.paint(PreviewScene{*key.document, *measured.value(), key.style, frame, 0.0F,
                               key.selection, images.get(), &bitmaps_, key.table_scrolls,
                               key.table_lit});
    if (failure.has_value())
    {
        return std::unexpected(failure.value());
    }
    return {};
}

std::expected<void, RenderFailure>
PreviewPane::hold_bitmaps(ID2D1RenderTarget &target, const core::MarkdownDocument &document,
                          const application::PreviewImages *images, const PreviewBlockTops &tops)
{
    // 製品の描く先（device context）と試験の WIC の描く先は、どちらも ID2D1DeviceContext を持つ。
    // 持たない描く先では bitmap を作らず、箱は空のまま描く（画像のために窓を閉じる道へ行かない）。
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> context;
    if (images == nullptr || FAILED(target.QueryInterface(IID_PPV_ARGS(&context))))
    {
        bitmaps_.release();
        return {};
    }
    bitmaps_.begin(*context.Get());
    const auto &all = document.images;
    for (auto at = std::ranges::lower_bound(all, tops.first(), {}, &core::MarkdownImage::block);
         at != all.end() && at->block < tops.size(); ++at)
    {
        const auto pixels =
            images->pixels_of(static_cast<std::size_t>(std::distance(all.begin(), at)));
        // device lost だけを返す。ほかの失敗（GPU の記憶の不足など）はその画像を飛ばし、箱は空の
        // まま描いて、次に描き直すときにまた作るのを試す。
        if (pixels != nullptr && preview_bitmap_lost(bitmaps_.hold(*context.Get(), pixels)))
        {
            return std::unexpected(RenderFailure::device_lost);
        }
    }
    return {};
}

void PreviewPane::release() noexcept
{
    bitmaps_.release();
    tops_.reset();
    measured_ = nullptr;
    measured_images_.reset();
    area_.reset();
    pictured_.reset();
    table_scrolls_.clear();
    settled_.reset();
}

const PreviewBitmaps &PreviewPane::bitmaps() const noexcept
{
    return bitmaps_;
}

ID2D1Bitmap1 *PreviewPane::picture_of(ID2D1DeviceContext &context, std::int32_t width,
                                      std::int32_t height)
{
    if (width <= 0 || height <= 0)
    {
        return nullptr;
    }
    const auto size = D2D1::SizeU(static_cast<UINT32>(width), static_cast<UINT32>(height));
    if (picture_ != nullptr && picture_->GetPixelSize().width == size.width &&
        picture_->GetPixelSize().height == size.height)
    {
        return picture_.Get();
    }
    picture_.Reset();
    float dpi_x = 0.0F;
    float dpi_y = 0.0F;
    context.GetDpi(&dpi_x, &dpi_y);
    // 描く先と同じ画素形式と alpha・同じ DPI（製品は 96 で 1 DIP = 1 物理画素）。
    const auto properties =
        D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, context.GetPixelFormat(), dpi_x, dpi_y);
    if (FAILED(context.CreateBitmap(size, nullptr, 0, properties, &picture_)))
    {
        picture_.Reset();
        return nullptr;
    }
    ++pictures_;
    return picture_.Get();
}

std::expected<void, RenderFailure> PreviewPane::record(ID2D1DeviceContext &context,
                                                       ID2D1Bitmap1 &picture, PreviewPictureKey key)
{
    Microsoft::WRL::ComPtr<ID2D1Image> previous;
    context.GetTarget(&previous);
    context.SetTarget(&picture);
    context.Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
    const auto drawn = draw_scene(context, key, core::LayoutRect{0, 0, key.width, key.height});
    context.SetTarget(previous.Get());
    if (!drawn)
    {
        return drawn;
    }
    pictured_.emplace(std::move(key));
    return {};
}

std::expected<void, RenderFailure> PreviewPane::paint(ID2D1RenderTarget &target,
                                                      const application::PreviewFrame &frame,
                                                      const PreviewTextStyle &style,
                                                      const core::LayoutRect &area)
{
    const auto &document = frame.document;
    area_.reset();
    settled_.reset();
    if (document == nullptr)
    {
        target.FillRectangle(
            D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                        static_cast<float>(area.right), static_cast<float>(area.bottom)),
            brushes_.of(PreviewInk::background));
        return {};
    }
    area_ = area;
    const auto width = core::width_of(area);
    const auto height = core::height_of(area);
    table_scrolls_ = frame.table_scrolls;
    const auto block =
        application::preview_anchored_block(*document, frame.anchor, frame.anchor_block);
    PreviewPictureKey key{
        &target,         document,   block,           width,        height,
        style,           theme_,     frame.selection, frame.images, frame.table_scrolls,
        frame.table_lit, frame.nudge};
    const auto folded = fold(frame, key);
    if (!folded)
    {
        return folded;
    }
    const auto to = key.anchor;
    const auto nudge = key.nudge;
    const auto painted = paint_key(target, std::move(key), area);
    if (painted && (frame.pending != 0 || to != block || nudge != frame.nudge))
    {
        settle(frame, to, nudge);
    }
    return painted;
}

std::expected<void, RenderFailure> PreviewPane::paint_key(ID2D1RenderTarget &target,
                                                          PreviewPictureKey key,
                                                          const core::LayoutRect &area)
{
    const auto width = core::width_of(area);
    const auto height = core::height_of(area);
    // 座標は物理画素の整数なので、拡大縮小せず最近傍で写せば滲まない。
    const auto destination =
        D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                    static_cast<float>(area.right), static_cast<float>(area.bottom));
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> context;
    if (FAILED(target.QueryInterface(IID_PPV_ARGS(&context))))
    {
        pictured_.reset();
        return draw_scene(target, key, area);
    }
    if (pictured_.has_value() && pictured_.value() == key && picture_ != nullptr)
    {
        ++reused_;
        context->DrawBitmap(picture_.Get(), destination, 1.0F,
                            D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, nullptr, nullptr);
        return {};
    }
    pictured_.reset();
    auto *const picture = picture_of(*context.Get(), width, height);
    if (picture == nullptr)
    {
        return draw_scene(target, key, area);
    }
    const auto recorded = record(*context.Get(), *picture, std::move(key));
    if (!recorded)
    {
        return recorded;
    }
    context->DrawBitmap(picture, destination, 1.0F, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                        nullptr, nullptr);
    return {};
}

std::expected<void, RenderFailure> PreviewPane::fold(const application::PreviewFrame &frame,
                                                     PreviewPictureKey &key)
{
    // 送りが無く、錨の要素の上端が枠の上端（追従の形）か、前に畳んで描いた鍵と同じなら畳まない。
    if (frame.pending == 0 &&
        (key.nudge == 0.0F || (pictured_.has_value() && pictured_.value() == key)))
    {
        return {};
    }
    const PreviewScrollSpot from{key.anchor, preview_pixels(key.nudge, key.style.dpi) +
                                                 static_cast<float>(frame.pending) *
                                                     preview_line_pixels(key.style)};
    const auto spot =
        preview_scroll_fold(from, key.document->blocks.size(), static_cast<float>(key.height),
                            [this, &key](std::size_t index) { return this->span_of(key, index); });
    if (!spot)
    {
        return std::unexpected(spot.error());
    }
    // 送りが無く畳んでも同じ位置なら DIP のまま残す（写し直しの丸めで答えを往復させない）。
    if (frame.pending != 0 || !(spot.value() == from))
    {
        key.anchor = spot.value().block;
        key.nudge = preview_dips(spot.value().nudge, key.style.dpi);
    }
    return {};
}

std::expected<float, RenderFailure> PreviewPane::span_of(const PreviewPictureKey &key,
                                                         std::size_t index)
{
    const auto images = key.images.lock();
    // 高さ 0 の枠で index だけを置き（through）、次の要素の上端は間だけを足す（まだ見えない次の
    // 要素の layout を作らない・レビュー R4）。
    const PreviewViewport viewport{
        index, static_cast<float>(key.width), 0.0F, images.get(), 0.0F, index + 1U};
    auto tops = PreviewBlockTops::measure(*key.document, layouts_, key.style, viewport);
    if (!tops)
    {
        return std::unexpected(tops.error());
    }
    auto &measured = tops.value();
    return measured.next_top(*key.document, key.style).value_or(measured.bottom(index));
}

void PreviewPane::settle(const application::PreviewFrame &frame, std::size_t block, float nudge)
{
    if (measured_ == nullptr || block >= measured_->blocks.size())
    {
        return;
    }
    settled_ = application::PreviewScrollSettled{block, measured_->blocks.at(block).source_begin,
                                                 code_line_at_top(block), nudge, frame.pending};
}

std::size_t PreviewPane::code_line_at_top(std::size_t index)
{
    if (!area_.has_value() || !tops_.has_value() || measured_ == nullptr ||
        index != tops_.value().first() ||
        measured_->blocks.at(index).kind != core::PreviewBlockKind::code)
    {
        return 0;
    }
    // 枠の上端の点を描く所と同じ layout で引き、その前の改行を数える。
    const auto &tops = tops_.value();
    const auto point =
        hit_block(tops, index, D2D1::Point2F(tops.indent(index), 0.0F), PreviewHitMode::edge);
    if (!point.has_value())
    {
        return 0;
    }
    const auto shown = core::preview_shown_text(measured_->blocks.at(index));
    const auto before = shown.substr(0, std::min(point.value().offset, shown.size()));
    return static_cast<std::size_t>(std::ranges::count(before, '\n'));
}

const std::optional<application::PreviewScrollSettled> &PreviewPane::settled() const noexcept
{
    return settled_;
}

std::optional<std::size_t> PreviewPane::task_at(std::int32_t x, std::int32_t y) const
{
    if (!area_.has_value() || !tops_.has_value() || !core::contains(area_.value(), x, y))
    {
        return std::nullopt;
    }
    const auto &area = area_.value();
    const auto &tops = tops_.value();
    for (auto index = tops.first(); index < tops.size(); ++index)
    {
        const auto &box = tops.task_box(index);
        if (box.has_value() && core::contains(box.value().target, x - area.left, y - area.top))
        {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<core::PreviewPoint> PreviewPane::point_at(std::int32_t x, std::int32_t y)
{
    if (!area_.has_value() || !tops_.has_value() || measured_ == nullptr ||
        tops_.value().first() >= tops_.value().size())
    {
        return std::nullopt;
    }
    const auto &area = area_.value();
    const auto &tops = tops_.value();
    const auto local = static_cast<float>(y - area.top);
    std::size_t index = tops.first();
    for (auto at = tops.first(); at < tops.size() && tops.top(at) <= local; ++at)
    {
        index = at;
    }
    const auto &block = measured_->blocks.at(index);
    if (local < tops.top(index) || block.kind == core::PreviewBlockKind::rule)
    {
        return core::PreviewPoint{index, 0};
    }
    if (local >= tops.bottom(index))
    {
        return core::PreviewPoint{index, core::preview_shown_text(block).size()};
    }
    return hit_block(tops, index, D2D1::Point2F(static_cast<float>(x - area.left), local),
                     PreviewHitMode::edge);
}

std::optional<core::PreviewPoint> PreviewPane::character_at(std::int32_t x, std::int32_t y)
{
    if (!area_.has_value() || !tops_.has_value() || measured_ == nullptr ||
        !core::contains(area_.value(), x, y))
    {
        return std::nullopt;
    }
    const auto &area = area_.value();
    const auto &tops = tops_.value();
    const auto local = static_cast<float>(y - area.top);
    for (auto index = tops.first(); index < tops.size(); ++index)
    {
        if (tops.top(index) <= local && local < tops.bottom(index))
        {
            return measured_->blocks.at(index).kind == core::PreviewBlockKind::rule
                       ? std::nullopt
                       : hit_block(tops, index,
                                   D2D1::Point2F(static_cast<float>(x - area.left), local),
                                   PreviewHitMode::character);
        }
    }
    return std::nullopt;
}

std::optional<core::LayoutRect> PreviewPane::range_box(std::size_t block, std::size_t begin,
                                                       std::size_t end)
{
    if (!area_.has_value() || !tops_.has_value() || measured_ == nullptr ||
        !measured_style_.has_value() || block < tops_.value().first() ||
        block >= tops_.value().size() || block >= measured_->blocks.size())
    {
        return std::nullopt;
    }
    const auto &style = measured_style_.value();
    const auto local = measured_->blocks.at(block).kind == core::PreviewBlockKind::table_row
                           ? cell_range_box(block, begin, end, style)
                           : text_range_of(block, begin, end, style);
    if (!local.has_value())
    {
        return std::nullopt;
    }
    const auto left = static_cast<float>(area_.value().left) + tops_.value().indent(block);
    const auto top = static_cast<float>(area_.value().top) + tops_.value().top(block);
    return core::LayoutRect{static_cast<std::int32_t>(std::floor(left + local.value().left)),
                            static_cast<std::int32_t>(std::floor(top + local.value().top)),
                            static_cast<std::int32_t>(std::ceil(left + local.value().right)),
                            static_cast<std::int32_t>(std::ceil(top + local.value().bottom))};
}

std::optional<D2D1_RECT_F> PreviewPane::text_range_of(std::size_t index, std::size_t begin,
                                                      std::size_t end,
                                                      const PreviewTextStyle &style)
{
    if (!area_.has_value() || !tops_.has_value())
    {
        return std::nullopt;
    }
    const auto &element = measured_->blocks.at(index);
    // 描く所（hit_block と同じ）の幅と原点（上の余白は切れの形に依る・#1863）。
    const auto inset = preview_paneled(element.kind) ? preview_code_padding(style.dpi) : 0.0F;
    const auto width =
        preview_text_width(element, static_cast<float>(core::width_of(area_.value())),
                           tops_.value().indent(index), style.dpi);
    const auto layout =
        layouts_.get(element, width, style,
                     preview_inline_images(*measured_, measured_images_.lock().get(), index,
                                           preview_dips_of(width, style.dpi)));
    if (!layout)
    {
        return std::nullopt;
    }
    const auto shown = core::preview_shown_text(element);
    const auto first = preview_units(shown, std::min(begin, shown.size()));
    const auto last = preview_units(shown, std::min(end, shown.size()));
    return preview_text_range_box(
        *layout.value().Get(), first, last - first,
        D2D1::Point2F(inset, preview_panel_top_inset(element, style.dpi)));
}

std::optional<core::PreviewPoint> PreviewPane::hit_block(const PreviewBlockTops &tops,
                                                         std::size_t index, D2D1_POINT_2F point,
                                                         PreviewHitMode mode)
{
    if (!measured_style_.has_value() || !area_.has_value())
    {
        return std::nullopt;
    }
    const auto &area = area_.value();
    const auto &style = measured_style_.value();
    const auto &block = measured_->blocks.at(index);
    if (block.kind == core::PreviewBlockKind::table_row)
    {
        return hit_cell(index,
                        D2D1::Point2F(point.x - tops.indent(index), point.y - tops.top(index)),
                        style, mode);
    }
    // 描く所（PreviewPainter::draw_text）と同じ幅と原点。面に描く要素は面の内側の余白だけ右へ、
    // 上の余白（上の余白を持たない切れは 0・#1863）だけ下げる。
    const auto inset = preview_paneled(block.kind) ? preview_code_padding(style.dpi) : 0.0F;
    const auto top_inset = preview_panel_top_inset(block, style.dpi);
    const auto width = preview_text_width(block, static_cast<float>(core::width_of(area)),
                                          tops.indent(index), style.dpi);
    // 描く所と同じ箱で同じ layout を引く（画像の上の点は箱の前か後ろ・画像は 1 つの単位）。
    const auto layout =
        layouts_.get(block, width, style,
                     preview_inline_images(*measured_, measured_images_.lock().get(), index,
                                           preview_dips_of(width, style.dpi)));
    if (!layout)
    {
        return std::nullopt;
    }
    const auto units = hit_units(
        *layout.value().Get(),
        D2D1::Point2F(point.x - tops.indent(index) - inset, point.y - tops.top(index) - top_inset),
        mode);
    if (!units.has_value())
    {
        return std::nullopt;
    }
    return core::PreviewPoint{
        index, preview_byte_of_units(core::preview_shown_text(block), units.value())};
}

std::optional<core::PreviewPoint> PreviewPane::hit_cell(std::size_t index, D2D1_POINT_2F point,
                                                        const PreviewTextStyle &style,
                                                        PreviewHitMode mode)
{
    const auto &block = measured_->blocks.at(index);
    if (!block.table.has_value())
    {
        return std::nullopt;
    }
    const auto &table = block.table.value();
    const auto columns = layouts_.table_columns(*measured_, index, style);
    if (!columns || table.cells.empty())
    {
        return std::nullopt;
    }
    const auto &widths = *columns.value();
    const auto count = std::min(table.cells.size(), widths.size());
    const auto padding = preview_table_padding_x(style.dpi);
    const auto shift = table_shift(index, widths, style);
    // 点が掛かる列（右の余白までを含める・最後の列より右は最後の列）。
    std::size_t column = 0;
    while (column + 1U < count &&
           point.x >= preview_table_cell_left(widths, column, shift, style.dpi) +
                          widths.at(column) + padding)
    {
        ++column;
    }
    const auto cell = core::preview_table_cell(block, column);
    const auto layout = layouts_.get(cell, preview_marker_layout_width(), style);
    DWRITE_TEXT_METRICS metrics{};
    if (!layout || FAILED(layout.value()->GetMetrics(&metrics)))
    {
        return std::nullopt;
    }
    const auto left =
        preview_table_cell_left(widths, column, shift, style.dpi) +
        preview_table_aligned(widths.at(column), metrics.width, table.aligns.at(column));
    const auto units = hit_units(
        *layout.value().Get(),
        D2D1::Point2F(point.x - left, point.y - preview_table_padding_y(style.dpi)), mode);
    if (!units.has_value())
    {
        return std::nullopt;
    }
    const auto first = core::preview_table_cell_range(block, column).first;
    return core::PreviewPoint{index, first + preview_byte_of_units(cell.text, units.value())};
}

std::optional<D2D1_RECT_F> PreviewPane::cell_range_box(std::size_t index, std::size_t begin,
                                                       std::size_t end,
                                                       const PreviewTextStyle &style)
{
    const auto &block = measured_->blocks.at(index);
    const auto columns = layouts_.table_columns(*measured_, index, style);
    if (!block.table.has_value() || !columns || block.table.value().cells.empty())
    {
        return std::nullopt;
    }
    const auto &table = block.table.value();
    const auto &widths = *columns.value();
    const auto count = std::min(table.cells.size(), widths.size());
    std::size_t column = 0;
    while (column + 1U < count && core::preview_table_cell_range(block, column + 1U).first <= begin)
    {
        ++column;
    }
    const auto [first, second] = core::preview_table_cell_range(block, column);
    const auto cell = core::preview_table_cell(block, column);
    const auto layout = layouts_.get(cell, preview_marker_layout_width(), style);
    DWRITE_TEXT_METRICS metrics{};
    if (!layout || FAILED(layout.value()->GetMetrics(&metrics)))
    {
        return std::nullopt;
    }
    const auto left =
        preview_table_cell_left(widths, column, table_shift(index, widths, style), style.dpi) +
        preview_table_aligned(widths.at(column), metrics.width, table.aligns.at(column));
    const auto from = preview_units(cell.text, std::clamp(begin, first, second) - first);
    const auto to = preview_units(cell.text, std::clamp(end, first, second) - first);
    return preview_text_range_box(*layout.value().Get(), from, to - from,
                                  D2D1::Point2F(left, preview_table_padding_y(style.dpi)));
}

float PreviewPane::table_shift(std::size_t index, std::span<const float> columns,
                               const PreviewTextStyle &style) const
{
    const auto &block = measured_->blocks.at(index);
    if (!block.table.has_value() || !area_.has_value() || !tops_.has_value())
    {
        return 0.0F;
    }
    return preview_table_shift(table_band(index, columns, style.dpi),
                               preview_table_offset(table_scrolls_, block.table.value().table),
                               style.dpi);
}

PreviewTableBand PreviewPane::table_band(std::size_t index, std::span<const float> columns,
                                         std::uint32_t dpi) const
{
    if (!area_.has_value() || !tops_.has_value())
    {
        return PreviewTableBand{0.0F, 0.0F};
    }
    return preview_table_band(columns, static_cast<float>(core::width_of(area_.value())),
                              tops_.value().indent(index), dpi);
}

std::optional<PreviewTableHit> PreviewPane::table_hit(std::size_t index, std::int32_t x,
                                                      float local)
{
    const auto &block = measured_->blocks.at(index);
    if (!block.table.has_value() || !measured_style_.has_value() || !area_.has_value() ||
        !tops_.has_value())
    {
        return std::nullopt;
    }
    const auto &style = measured_style_.value();
    const auto columns = layouts_.table_columns(*measured_, index, style);
    if (!columns)
    {
        return std::nullopt;
    }
    const auto band = table_band(index, *columns.value(), style.dpi);
    if (!preview_table_overflows(band, style.dpi))
    {
        return std::nullopt;
    }
    const auto &tops = tops_.value();
    const auto track = static_cast<float>(x - area_.value().left) - tops.indent(index);
    const bool bar = preview_table_closes(*measured_, index) &&
                     local >= tops.bottom(index) - preview_table_bar_extent(style.dpi);
    return PreviewTableHit{block.table.value().table, preview_table_span(band, style.dpi),
                           preview_table_track_dips(band, track, style.dpi), bar};
}

std::optional<PreviewTableHit> PreviewPane::table_at(std::int32_t x, std::int32_t y)
{
    if (!area_.has_value() || !tops_.has_value() || measured_ == nullptr ||
        !core::contains(area_.value(), x, y))
    {
        return std::nullopt;
    }
    const auto &tops = tops_.value();
    const auto local = static_cast<float>(y - area_.value().top);
    for (auto index = tops.first(); index < tops.size(); ++index)
    {
        if (tops.top(index) <= local && local < tops.bottom(index))
        {
            const auto hit = table_hit(index, x, local);
            // 表の上は行の原点から枠の右の余白まで（棒の溝と同じ幅）。
            const bool inside =
                hit.has_value() && hit.value().at >= 0 && hit.value().at < hit.value().span.view;
            return inside ? hit : std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<PreviewTableHit> PreviewPane::table_bar(std::size_t table, std::int32_t x)
{
    if (!area_.has_value() || !tops_.has_value() || measured_ == nullptr)
    {
        return std::nullopt;
    }
    const auto &tops = tops_.value();
    for (auto index = tops.first(); index < tops.size(); ++index)
    {
        const auto &row = measured_->blocks.at(index).table;
        if (row.has_value() && row.value().table == table)
        {
            return table_hit(index, x, tops.top(index));
        }
    }
    return std::nullopt;
}

std::size_t PreviewPane::created() const noexcept
{
    return layouts_.created();
}

std::size_t PreviewPane::painted() const noexcept
{
    return painted_;
}

std::size_t PreviewPane::reused() const noexcept
{
    return reused_;
}

std::size_t PreviewPane::pictures() const noexcept
{
    return pictures_;
}
} // namespace nenenib::ui::win32
