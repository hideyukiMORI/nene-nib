#include "ExplorerShapes.hpp"

#include "LayoutRect.hpp"

#include <algorithm>

namespace nenenib::ui::win32
{
ID2D1StrokeStyle *ExplorerShapes::stroke(ID2D1Factory &factory)
{
    if (factory_.Get() != &factory)
    {
        clear();
        factory_ = &factory;
    }
    if (!stroke_tried_)
    {
        stroke_tried_ = true;
        const auto round = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                                                       D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND);
        if (FAILED(factory.CreateStrokeStyle(&round, nullptr, 0, stroke_.ReleaseAndGetAddressOf())))
        {
            stroke_.Reset();
        }
        ++made_;
    }
    return stroke_.Get();
}

bool ExplorerShapes::corner(ID2D1Factory &factory, const core::ExplorerLayout &layout)
{
    static_cast<void>(stroke(factory));
    const core::LayoutRect panel = layout.panel.value_or(core::LayoutRect{0, 0, 0, 0});
    const std::int32_t radius = std::min(layout.corner, core::height_of(panel));
    if (radius <= 0)
    {
        return false;
    }
    if (outside_ != nullptr && arc_ != nullptr && right_ == panel.right && top_ == panel.top &&
        radius_ == radius && border_ == layout.border)
    {
        return true;
    }
    right_ = panel.right;
    top_ = panel.top;
    radius_ = radius;
    border_ = layout.border;
    ++made_;
    if (build_corner(factory))
    {
        return true;
    }
    outside_.Reset();
    arc_.Reset();
    return false;
}

// 角の外（面の右端・上端から半径の弧まで）を塗る面と、境界の続きの弧。寸法は覚えた鍵から読む。
bool ExplorerShapes::build_corner(ID2D1Factory &factory)
{
    const auto left = static_cast<float>(right_);
    const auto top = static_cast<float>(top_);
    const auto radius = static_cast<float>(radius_);
    const auto border = static_cast<float>(border_);
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(factory.CreatePathGeometry(outside_.ReleaseAndGetAddressOf())) ||
        FAILED(outside_->Open(sink.GetAddressOf())))
    {
        return false;
    }
    const float half = border / 2.0F;
    sink->BeginFigure(D2D1::Point2F(left, top), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(left + radius, top));
    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(left, top + radius), D2D1::SizeF(radius, radius),
                                  0.0F, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                                  D2D1_ARC_SIZE_SMALL));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    if (FAILED(sink->Close()) ||
        FAILED(factory.CreatePathGeometry(arc_.ReleaseAndGetAddressOf())) ||
        FAILED(arc_->Open(sink.ReleaseAndGetAddressOf())))
    {
        return false;
    }
    sink->BeginFigure(D2D1::Point2F(left - half, top + radius), D2D1_FIGURE_BEGIN_HOLLOW);
    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(left + radius, top - half),
                                  D2D1::SizeF(radius + half, radius + half), 0.0F,
                                  D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    return SUCCEEDED(sink->Close());
}

ID2D1PathGeometry *ExplorerShapes::outside() const noexcept
{
    return outside_.Get();
}

ID2D1PathGeometry *ExplorerShapes::arc() const noexcept
{
    return arc_.Get();
}

std::size_t ExplorerShapes::made() const noexcept
{
    return made_;
}

void ExplorerShapes::clear() noexcept
{
    factory_.Reset();
    stroke_.Reset();
    outside_.Reset();
    arc_.Reset();
    right_ = 0;
    top_ = 0;
    radius_ = 0;
    border_ = 0;
    stroke_tried_ = false;
}
} // namespace nenenib::ui::win32
