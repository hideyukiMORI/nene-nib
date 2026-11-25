#pragma once

#include "InvisibleBox.hpp"
#include "InvisibleKind.hpp"
#include "LineBreakMark.hpp"
#include "LineBreakPlace.hpp"

#include <algorithm>
#include <d2d1.h>
#include <utility>

namespace nenenib::ui::win32
{
// 不可視文字の印（ADR 0193 の決定 10・施主の決めた見本
// A）。字体の字ではなく線と点で描く（字体に依らず 高さと太さがそろう）。大きさは見本の 22
// 画素の行を 1 単位（行の高さ / 22）として決め、太さと濃さの 値はこの 1 か所に置く（ADR
// の「未決」・試用で変える）。色は呼び手が brush に置いてから呼ぶ。 Target は製品では
// ID2D1RenderTarget、tests/ui はメモリの bitmap の描画先。
class InvisibleMarkPainter final
{
  public:
    // 見本の行の高さ（この値を 1 として印の大きさを決める）。
    static constexpr float sample_height = 22.0F;
    // 線の太さ（見本の単位・1 物理画素を下回らない）。
    static constexpr float stroke_units = 1.2F;
    // 空白の点の半径（見本の単位）。
    static constexpr float dot_units = 1.1F;

    // 1 字の升に印を描く（空白と行末の空白は中央の点、Tab は横線と右端の矢じり、全角空白は内側の
    // 四角の輪郭）。box は描く先の座標。
    template <typename Target>
    static void paint(Target &target, ID2D1Brush *brush, const InvisibleBox &box, float height)
    {
        const float unit = height / sample_height;
        const float stroke = std::max(1.0F, stroke_units * unit);
        const auto &rect = box.box;
        const float middle = (rect.top + rect.bottom) / 2.0F;
        switch (box.kind)
        {
        case core::InvisibleKind::space:
        case core::InvisibleKind::trailing_space:
        {
            const float radius = std::max(0.75F, dot_units * unit);
            target.FillEllipse(D2D1::Ellipse(D2D1::Point2F((rect.left + rect.right) / 2.0F, middle),
                                             radius, radius),
                               brush);
            return;
        }
        case core::InvisibleKind::tab:
            paint_tab(target, brush, rect, std::pair{unit, stroke});
            return;
        case core::InvisibleKind::ideographic_space:
        {
            const float side = std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.55F;
            const float center = (rect.left + rect.right) / 2.0F;
            target.DrawRectangle(D2D1::RectF(center - (side / 2.0F), middle - (side / 2.0F),
                                             center + (side / 2.0F), middle + (side / 2.0F)),
                                 brush, stroke);
            return;
        }
        }
    }

    // 行の終わりの改行の印（`↵`・CRLF はその前に小さな `←`）。見本の 10 × 22 の升の線を、空白の幅と
    // 行の高さの小さい方の比で縮めて縦の中央に置く。
    template <typename Target>
    static void paint_line_break(Target &target, ID2D1Brush *brush, const LineBreakPlace &place)
    {
        if (place.space <= 0 || place.height <= 0)
        {
            return;
        }
        const float unit = std::min(place.space / cell_units, place.height / sample_height);
        const float stroke = std::max(1.0F, stroke_units * unit);
        const float top = place.at.y + ((place.height - (sample_height * unit)) / 2.0F);
        const auto point = [&place, unit, top](float x, float y)
        { return D2D1::Point2F(place.at.x + (x * unit), top + (y * unit)); };
        float shift = 0;
        switch (place.mark)
        {
        case core::LineBreakMark::crlf:
            // `←`: (8, 11) → (2, 11) と矢じり (4.5, 8.5) → (2, 11) → (4.5, 13.5)。
            target.DrawLine(point(8, 11), point(2, 11), brush, stroke);
            target.DrawLine(point(4.5F, 8.5F), point(2, 11), brush, stroke);
            target.DrawLine(point(4.5F, 13.5F), point(2, 11), brush, stroke);
            shift = cell_units;
            break;
        case core::LineBreakMark::lf:
            break;
        }
        // `↵`: (8, 7) → (8, 13) → (2, 13) と矢じり (4.5, 10.5) → (2, 13) → (4.5, 15.5)。
        target.DrawLine(point(shift + 8, 7), point(shift + 8, 13), brush, stroke);
        target.DrawLine(point(shift + 8, 13), point(shift + 2, 13), brush, stroke);
        target.DrawLine(point(shift + 4.5F, 10.5F), point(shift + 2, 13), brush, stroke);
        target.DrawLine(point(shift + 4.5F, 15.5F), point(shift + 2, 13), brush, stroke);
    }

    // 改行の印の幅（LF は見本の 1 升・CRLF は 2 升・paint_line_break と同じ縮め方）。
    [[nodiscard]] static float line_break_width(const LineBreakPlace &place) noexcept
    {
        const float unit = std::min(place.space / cell_units, place.height / sample_height);
        switch (place.mark)
        {
        case core::LineBreakMark::crlf:
            return 2.0F * cell_units * unit;
        case core::LineBreakMark::lf:
            return cell_units * unit;
        }
        std::unreachable();
    }

  private:
    // 見本の 1 字の升の幅（見本の単位・空白の幅に当たる）。
    static constexpr float cell_units = 10.0F;

    // Tab の横線と右端の矢じり。左右に見本の 4 と 3 の余白（Tab の幅の 1/4 と 1/5 を超えない）。
    // scale は（見本の単位, 線の太さ）。
    template <typename Target>
    static void paint_tab(Target &target, ID2D1Brush *brush, const D2D1_RECT_F &rect,
                          std::pair<float, float> scale)
    {
        const auto [unit, stroke] = scale;
        const float width = rect.right - rect.left;
        const float from = rect.left + std::min(4.0F * unit, width / 4.0F);
        const float to = rect.right - std::min(3.0F * unit, width / 5.0F);
        const float middle = (rect.top + rect.bottom) / 2.0F;
        const float head = std::min(2.5F * unit, (to - from) / 2.0F);
        target.DrawLine(D2D1::Point2F(from, middle), D2D1::Point2F(to, middle), brush, stroke);
        target.DrawLine(D2D1::Point2F(to - head, middle - head), D2D1::Point2F(to, middle), brush,
                        stroke);
        target.DrawLine(D2D1::Point2F(to - head, middle + head), D2D1::Point2F(to, middle), brush,
                        stroke);
    }
};
} // namespace nenenib::ui::win32
