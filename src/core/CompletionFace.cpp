#include "CompletionFace.hpp"

#include "DevicePixels.hpp"

#include <algorithm>

namespace nenenib::core
{
namespace
{
[[nodiscard]] std::int32_t height_for(std::size_t rows, std::uint32_t dpi) noexcept
{
    return (to_pixels(completion_face_gap_dips, dpi) * 2) +
           (to_pixels(completion_face_row_dips, dpi) * static_cast<std::int32_t>(rows));
}

// room の高さに入る行の数（1 以上・wanted 以下）。
[[nodiscard]] std::size_t rows_within(std::int32_t room, std::size_t wanted,
                                      std::uint32_t dpi) noexcept
{
    const auto inside = room - (to_pixels(completion_face_gap_dips, dpi) * 2);
    const auto fitted = std::max(inside / to_pixels(completion_face_row_dips, dpi), 1);
    return std::min(wanted, static_cast<std::size_t>(fitted));
}

[[nodiscard]] std::int32_t placed_left(const CompletionFaceRequest &request,
                                       std::int32_t width) noexcept
{
    const auto last = std::max(request.bounds.left, request.bounds.right - width);
    return std::clamp(request.x, request.bounds.left, last);
}
} // namespace

CompletionFaceLayout completion_face_layout(const CompletionFaceRequest &request) noexcept
{
    const auto dpi = request.dpi;
    const auto gap = to_pixels(completion_face_caret_gap_dips, dpi);
    const auto below = request.bounds.bottom - (request.caret.bottom + gap);
    const auto over = (request.caret.top - gap) - request.bounds.top;
    const std::size_t wanted = std::min(request.rows, completion_face_rows);
    const bool fits_below = height_for(wanted, dpi) <= below;
    const bool above = !fits_below && (height_for(wanted, dpi) <= over || over > below);
    const std::size_t rows = rows_within(above ? over : below, wanted, dpi);
    const auto width =
        to_pixels(request.detailed ? completion_face_wide_dips : completion_face_width_dips, dpi);
    const auto left = placed_left(request, width);
    const auto height = height_for(rows, dpi);
    const auto top = above ? request.caret.top - gap - height : request.caret.bottom + gap;
    // 選んでいる行が無い（selected が rows 以上・Vim の補完が元の接頭辞へ戻った 1 歩・#1845）ときは
    // 先頭から出す。
    const bool chosen = request.selected < request.rows;
    const std::size_t first = chosen && request.selected >= rows ? request.selected + 1 - rows : 0;
    return CompletionFaceLayout{LayoutRect{left, top, left + width, top + height},
                                first,
                                rows,
                                above,
                                request.detailed,
                                dpi};
}

LayoutRect completion_face_row(const CompletionFaceLayout &layout, std::size_t index) noexcept
{
    const auto height = to_pixels(completion_face_row_dips, layout.dpi);
    const auto top = layout.panel.top + to_pixels(completion_face_gap_dips, layout.dpi) +
                     (height * static_cast<std::int32_t>(index));
    return LayoutRect{layout.panel.left, top, layout.panel.right, top + height};
}

LayoutRect completion_face_mark(const CompletionFaceLayout &layout, std::size_t index) noexcept
{
    const auto row = completion_face_row(layout, index);
    const auto left = row.left + to_pixels(completion_face_padding_dips, layout.dpi);
    return LayoutRect{left, row.top, left + to_pixels(completion_face_mark_dips, layout.dpi),
                      row.bottom};
}

LayoutRect completion_face_label(const CompletionFaceLayout &layout, std::size_t index) noexcept
{
    const auto row = completion_face_row(layout, index);
    const auto padding = to_pixels(completion_face_padding_dips, layout.dpi);
    const auto left = row.left + padding + to_pixels(completion_face_mark_dips, layout.dpi);
    // 補足の欄があるときの名前は、補足の無い面と同じ幅まで（広げた分が補足の欄）。
    const auto right = layout.detailed
                           ? row.left + to_pixels(completion_face_width_dips, layout.dpi) - padding
                           : row.right - padding;
    return LayoutRect{left, row.top, std::max(left, right), row.bottom};
}

LayoutRect completion_face_detail(const CompletionFaceLayout &layout, std::size_t index) noexcept
{
    const auto row = completion_face_row(layout, index);
    const auto padding = to_pixels(completion_face_padding_dips, layout.dpi);
    const auto right = row.right - padding;
    const auto left =
        layout.detailed ? row.left + to_pixels(completion_face_width_dips, layout.dpi) : right;
    return LayoutRect{std::min(left, right), row.top, right, row.bottom};
}
} // namespace nenenib::core
