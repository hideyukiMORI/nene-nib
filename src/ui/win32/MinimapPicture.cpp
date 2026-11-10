#include "MinimapPicture.hpp"

#include "Minimap.hpp"
#include "SyntaxColor.hpp"
#include "SyntaxRole.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr float full_channel = 255.0F;
// 見えている範囲の面の不透明度（決定 2・色のトークンは足さない）。
constexpr float face_opacity = 0.1F;
// bitmap の行の数はこの刻みで切り上げる（打鍵で行が増えるたびに作り直さない）。
constexpr std::size_t picture_row_step = 512;
constexpr std::size_t background_color = 0;
constexpr std::size_t face_color = 1;
constexpr std::size_t muted_color = 2;
constexpr std::size_t first_role_color = 3;
constexpr std::array<core::SyntaxRole, minimap_color_count - first_role_color> roles{
    core::SyntaxRole::comment,  core::SyntaxRole::keyword,   core::SyntaxRole::string,
    core::SyntaxRole::number,   core::SyntaxRole::type,      core::SyntaxRole::function,
    core::SyntaxRole::constant, core::SyntaxRole::operators, core::SyntaxRole::error,
    core::SyntaxRole::warning};

[[nodiscard]] D2D1_COLOR_F to_color(core::RgbColor color) noexcept
{
    return D2D1::ColorF(static_cast<float>(color.red) / full_channel,
                        static_cast<float>(color.green) / full_channel,
                        static_cast<float>(color.blue) / full_channel, 1.0F);
}

[[nodiscard]] float row_y(std::size_t row) noexcept
{
    return static_cast<float>(row * static_cast<std::size_t>(core::minimap_row_pixels));
}

[[nodiscard]] UINT32 row_pixels(std::size_t row) noexcept
{
    return static_cast<UINT32>(row * static_cast<std::size_t>(core::minimap_row_pixels));
}

[[nodiscard]] std::size_t color_of(const std::optional<core::SyntaxRole> &role) noexcept
{
    return role.has_value() ? first_role_color + static_cast<std::size_t>(role.value())
                            : muted_color;
}

[[nodiscard]] D2D1_RECT_F rect_of(const core::LayoutRect &area) noexcept
{
    return D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                       static_cast<float>(area.right), static_cast<float>(area.bottom));
}

// 面の最上段の行が窓の何行目か（窓は最上段を含む・core::minimap_window）と、面に出す行の数。
[[nodiscard]] std::pair<std::size_t, std::size_t> shown_rows(const application::MinimapFrame &frame,
                                                             const core::LayoutRect &panel) noexcept
{
    const auto &rows = *frame.rows;
    const std::size_t offset = std::min(
        frame.top.value - std::min(frame.top.value, rows.window.first.value), rows.rows.size());
    const auto fit =
        static_cast<std::size_t>(std::max(core::height_of(panel), 0) / core::minimap_row_pixels);
    return {offset, std::min(fit, rows.rows.size() - offset)};
}

void clear_rows(ID2D1DeviceContext &context, float width, std::size_t from, std::size_t to)
{
    context.PushAxisAlignedClip(D2D1::RectF(0.0F, row_y(from), width, row_y(to)),
                                D2D1_ANTIALIAS_MODE_ALIASED);
    context.Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
    context.PopAxisAlignedClip();
}

void fill_ground(const MinimapPen &pen, const core::LayoutRect &panel)
{
    pen.brush.SetColor(to_color(pen.colors.at(background_color)));
    pen.target.FillRectangle(rect_of(panel), &pen.brush);
}

// 1 行の塊（区間ごとに 1 つの FillRectangle）。current は brush
// に置いてある色で、変わるときだけ置く。
void draw_row(const MinimapPen &pen, const core::MinimapRow &row, D2D1_POINT_2F origin,
              std::optional<std::size_t> &current)
{
    for (const auto &block : row)
    {
        const std::size_t color = color_of(block.role);
        if (current != color)
        {
            pen.brush.SetColor(to_color(pen.colors.at(color)));
            current = color;
        }
        const float left = origin.x + static_cast<float>(block.left);
        pen.target.FillRectangle(
            D2D1::RectF(left, origin.y, left + static_cast<float>(block.width),
                        origin.y + static_cast<float>(core::minimap_row_pixels)),
            &pen.brush);
    }
}

// 帯が前の行の数と今の行の数と bitmap の高さに収まるか。
[[nodiscard]] bool band_fits(const core::MinimapBand &band, std::size_t now, std::size_t before,
                             UINT32 height) noexcept
{
    return row_pixels(std::max(now, before)) <= height && band.begin + band.inserted <= now &&
           band.begin + band.removed <= before;
}
} // namespace

MinimapColors minimap_colors(const core::Palette &palette, const core::SyntaxPalette &body) noexcept
{
    MinimapColors colors{};
    colors.at(background_color) = palette.background;
    colors.at(face_color) = palette.accent;
    colors.at(muted_color) = palette.muted;
    for (std::size_t index = 0; index < roles.size(); ++index)
    {
        colors.at(first_role_color + index) = core::syntax_color(body, roles.at(index));
    }
    return colors;
}

void MinimapPicture::draw_rows(const MinimapPen &pen, std::span<const core::MinimapRow> rows,
                               D2D1_POINT_2F origin)
{
    std::optional<std::size_t> current;
    D2D1_POINT_2F at = origin;
    for (const auto &row : rows)
    {
        draw_row(pen, row, at, current);
        at.y += static_cast<float>(core::minimap_row_pixels);
    }
}

void MinimapPicture::draw_face(const MinimapPen &pen, const application::MinimapFrame &frame,
                               const core::LayoutRect &panel, std::size_t shown)
{
    const std::size_t from = std::min(
        frame.first_visible.value - std::min(frame.first_visible.value, frame.top.value), shown);
    const std::size_t to = std::min(from + frame.visible, shown);
    pen.brush.SetColor(to_color(pen.colors.at(face_color)));
    pen.brush.SetOpacity(face_opacity);
    const auto top = static_cast<float>(panel.top);
    pen.target.FillRectangle(D2D1::RectF(static_cast<float>(panel.left), top + row_y(from),
                                         static_cast<float>(panel.right), top + row_y(to)),
                             &pen.brush);
    pen.brush.SetOpacity(1.0F);
}

void MinimapPicture::paint_directly(const MinimapPen &pen, const application::MinimapFrame &frame,
                                    const core::LayoutRect &panel)
{
    fill_ground(pen, panel);
    if (frame.rows == nullptr)
    {
        return;
    }
    const auto [offset, shown] = shown_rows(frame, panel);
    pen.target.PushAxisAlignedClip(rect_of(panel), D2D1_ANTIALIAS_MODE_ALIASED);
    draw_rows(pen, std::span(frame.rows->rows).subspan(offset, shown),
              D2D1::Point2F(static_cast<float>(panel.left), static_cast<float>(panel.top)));
    pen.target.PopAxisAlignedClip();
    painted_rows_ += shown;
    draw_face(pen, frame, panel, shown);
}

ID2D1Bitmap1 *MinimapPicture::picture_of(ID2D1DeviceContext &context, std::int32_t width,
                                         std::size_t rows)
{
    if (width <= 0)
    {
        return nullptr;
    }
    if (picture_ != nullptr && picture_->GetPixelSize().width == static_cast<UINT32>(width) &&
        picture_->GetPixelSize().height >= row_pixels(rows))
    {
        return picture_.Get();
    }
    picture_.Reset();
    scratch_.Reset();
    held_.reset();
    const std::size_t capacity = (std::max<std::size_t>(rows, 1) + picture_row_step - 1) /
                                 picture_row_step * picture_row_step;
    float dpi_x = 0.0F;
    float dpi_y = 0.0F;
    context.GetDpi(&dpi_x, &dpi_y);
    // 描く先と同じ画素形式と alpha・同じ DPI（製品は 96 で 1 DIP = 1 物理画素・#907 と同じ）。
    const auto properties =
        D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, context.GetPixelFormat(), dpi_x, dpi_y);
    const auto size = D2D1::SizeU(static_cast<UINT32>(width), row_pixels(capacity));
    if (FAILED(context.CreateBitmap(size, nullptr, 0, properties, &picture_)))
    {
        picture_.Reset();
        return nullptr;
    }
    ++pictures_;
    return picture_.Get();
}

void MinimapPicture::record(ID2D1DeviceContext &context, const MinimapPen &pen,
                            const application::MinimapRows &rows)
{
    Microsoft::WRL::ComPtr<ID2D1Image> previous;
    context.GetTarget(&previous);
    context.SetTarget(picture_.Get());
    context.Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
    draw_rows(MinimapPen{context, pen.brush, pen.colors}, rows.rows, D2D1::Point2F(0.0F, 0.0F));
    context.SetTarget(previous.Get());
    painted_rows_ += rows.rows.size();
}

bool MinimapPicture::shift_rows(ID2D1DeviceContext &context, const core::MinimapBand &band,
                                std::size_t before)
{
    const auto size = picture_->GetPixelSize();
    const std::size_t tail = before - band.begin - band.removed;
    if (band.removed == band.inserted || tail == 0)
    {
        return true;
    }
    if (scratch_ == nullptr)
    {
        float dpi_x = 0.0F;
        float dpi_y = 0.0F;
        context.GetDpi(&dpi_x, &dpi_y);
        const auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,
                                                        picture_->GetPixelFormat(), dpi_x, dpi_y);
        if (FAILED(context.CreateBitmap(size, nullptr, 0, properties, &scratch_)))
        {
            scratch_.Reset();
            return false;
        }
    }
    // 下の行を前の位置から新しい位置へ（同じ bitmap の重なる範囲は写せないので scratch を経る）。
    const D2D1_POINT_2U origin{0, 0};
    const D2D1_RECT_U moved{0, row_pixels(band.begin + band.removed), size.width,
                            row_pixels(before)};
    const D2D1_POINT_2U shifted{0, row_pixels(band.begin + band.inserted)};
    const D2D1_RECT_U staged{0, 0, size.width, row_pixels(tail)};
    if (FAILED(scratch_->CopyFromBitmap(&origin, picture_.Get(), &moved)) ||
        FAILED(picture_->CopyFromBitmap(&shifted, scratch_.Get(), &staged)))
    {
        return false;
    }
    ++shifts_;
    return true;
}

bool MinimapPicture::redraw_band(ID2D1DeviceContext &context, const MinimapPen &pen,
                                 const application::MinimapRows &rows,
                                 const core::MinimapBand &band)
{
    const std::size_t now = rows.rows.size();
    const std::size_t before = now + band.removed - band.inserted;
    const auto size = picture_->GetPixelSize();
    // 前に bitmap へ描いた命令を流してから写す。
    if (!band_fits(band, now, before, size.height) || FAILED(context.Flush()) ||
        !shift_rows(context, band, before))
    {
        return false;
    }
    Microsoft::WRL::ComPtr<ID2D1Image> previous;
    context.GetTarget(&previous);
    context.SetTarget(picture_.Get());
    const auto width = static_cast<float>(size.width);
    clear_rows(context, width, band.begin, band.begin + band.inserted);
    // 行が減ったら、ずらした後の下に残った前の行を消す。
    clear_rows(context, width, now, std::max(now, before));
    draw_rows(MinimapPen{context, pen.brush, pen.colors},
              std::span(rows.rows).subspan(band.begin, band.inserted),
              D2D1::Point2F(0.0F, row_y(band.begin)));
    context.SetTarget(previous.Get());
    painted_rows_ += band.inserted;
    return true;
}

void MinimapPicture::bring_up(ID2D1DeviceContext &context, const MinimapPen &pen,
                              const application::MinimapFrame &frame)
{
    const auto &rows = *frame.rows;
    const bool same = held_ == frame.rows && held_colors_ == pen.colors;
    const bool current = same && held_serial_ == rows.serial;
    const bool banded = same && !current && held_serial_ == frame.base && frame.band.has_value() &&
                        redraw_band(context, pen, rows, frame.band.value());
    if (!current && !banded)
    {
        record(context, pen, rows);
    }
    held_ = frame.rows;
    held_serial_ = rows.serial;
    held_colors_ = pen.colors;
}

std::expected<void, RenderFailure> MinimapPicture::paint(const MinimapPen &pen,
                                                         const application::MinimapFrame &frame,
                                                         const core::LayoutRect &panel)
{
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> context;
    auto *const picture =
        frame.rows != nullptr && SUCCEEDED(pen.target.QueryInterface(IID_PPV_ARGS(&context)))
            ? picture_of(*context.Get(), core::width_of(panel), frame.rows->rows.size())
            : nullptr;
    if (picture == nullptr)
    {
        held_.reset();
        paint_directly(pen, frame, panel);
        return {};
    }
    bring_up(*context.Get(), pen, frame);
    fill_ground(pen, panel);
    const auto [offset, shown] = shown_rows(frame, panel);
    const auto width = static_cast<float>(core::width_of(panel));
    const auto left = static_cast<float>(panel.left);
    const auto top = static_cast<float>(panel.top);
    const auto destination = D2D1::RectF(left, top, left + width, top + row_y(shown));
    const auto source = D2D1::RectF(0.0F, row_y(offset), width, row_y(offset + shown));
    // 座標は物理画素の整数なので、拡大縮小せず最近傍で写せば滲まない（#907 と同じ）。
    context->DrawBitmap(picture, destination, 1.0F, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                        &source, nullptr);
    ++copies_;
    draw_face(pen, frame, panel, shown);
    return {};
}

std::size_t MinimapPicture::painted_rows() const noexcept
{
    return painted_rows_;
}

std::size_t MinimapPicture::copies() const noexcept
{
    return copies_;
}

std::size_t MinimapPicture::pictures() const noexcept
{
    return pictures_;
}

std::size_t MinimapPicture::shifts() const noexcept
{
    return shifts_;
}
} // namespace nenenib::ui::win32
