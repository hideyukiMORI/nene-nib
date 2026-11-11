#include "ExplorerShapes.hpp"

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

std::size_t ExplorerShapes::made() const noexcept
{
    return made_;
}

void ExplorerShapes::clear() noexcept
{
    factory_.Reset();
    stroke_.Reset();
    stroke_tried_ = false;
}
} // namespace nenenib::ui::win32
