#pragma once

#include "CaretGlide.hpp"
#include "CaretMotionPaint.hpp"
#include "CaretQuad.hpp"

#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <d2d1.h>

namespace nenenib::ui::win32
{
// 動いている間のキャレット（尾を引く・ADR 0192 の決定 4）。本文の字形の上に重ねる層で、
// BodyTextPainter の字形と BodyTextLayouts の保持に触れない。四隅の多角形を尾の側の色から頭の色への
// 勾配で塗り、にじみは頭の色の半透明の多角形を外へ広げて 3 枚重ねる（Direct2D の効果は使わない）。
// Target は ID2D1RenderTarget の同じ名前のメソッドを持つ描画先（試験はメモリの
// bitmap）。色は呼び手が palette のトークンから渡す（ui に色のリテラルを書かない・ADR 0008）。
class CaretMotionPainter final
{
  public:
    // にじみの層（外から内へ）の広げる量（物理画素の glow に掛ける係数）と不透明度。
    static constexpr std::array<float, 3> glow_reach{1.0F, 0.6F, 0.3F};
    static constexpr std::array<float, 3> glow_alpha{0.08F, 0.12F, 0.18F};
    // 本体の不透明度（見本の 0.85 を縦棒でも読めるよう少し上げた値）。
    static constexpr float body_alpha = 0.9F;

    // 描けなければ（geometry や brush を作れない）偽。描けなくても frame は続ける（呼び手は次の
    // frame で同じ道を通る）。
    template <typename Target>
    [[nodiscard]] static bool paint(Target &target, const CaretMotionPaint &picture)
    {
        Microsoft::WRL::ComPtr<ID2D1Factory> factory;
        target.GetFactory(&factory);
        if (factory == nullptr)
        {
            return false;
        }
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> halo;
        if (FAILED(target.CreateSolidColorBrush(picture.head, &halo)))
        {
            return false;
        }
        for (std::size_t layer = 0; layer < glow_reach.size(); ++layer)
        {
            const auto geometry = polygon(
                *factory.Get(),
                core::caret_quad_inflated(picture.shown, picture.glow * glow_reach.at(layer)));
            if (geometry == nullptr)
            {
                return false;
            }
            halo->SetOpacity(glow_alpha.at(layer));
            target.FillGeometry(geometry.Get(), halo.Get());
        }
        const auto body = polygon(*factory.Get(), picture.shown);
        const auto brush = trail_brush(target, picture);
        if (body == nullptr || brush == nullptr)
        {
            return false;
        }
        brush->SetOpacity(body_alpha);
        target.FillGeometry(body.Get(), brush.Get());
        return true;
    }

  private:
    [[nodiscard]] static Microsoft::WRL::ComPtr<ID2D1PathGeometry>
    polygon(ID2D1Factory &factory, const core::CaretQuad &quad)
    {
        Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
        if (FAILED(factory.CreatePathGeometry(&geometry)) || FAILED(geometry->Open(&sink)))
        {
            return nullptr;
        }
        const auto &first = quad.corners.at(0);
        sink->BeginFigure(D2D1::Point2F(first.x, first.y), D2D1_FIGURE_BEGIN_FILLED);
        for (std::size_t index = 1; index < quad.corners.size(); ++index)
        {
            const auto &corner = quad.corners.at(index);
            sink->AddLine(D2D1::Point2F(corner.x, corner.y));
        }
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        if (FAILED(sink->Close()))
        {
            return nullptr;
        }
        return geometry;
    }

    // 尾の側（出発の中心）が tail、頭の側（行き先の中心）が head の線形の勾配。中心が同じなら
    // 頭の色の 1 色（勾配の向きが無い）。
    template <typename Target>
    [[nodiscard]] static Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush>
    trail_brush(Target &target, const CaretMotionPaint &picture)
    {
        const auto start = middle(picture.from);
        auto end = middle(picture.to);
        auto tail = picture.tail;
        if (start.x == end.x && start.y == end.y)
        {
            tail = picture.head;
            end.x += 1.0F;
        }
        const std::array<D2D1_GRADIENT_STOP, 2> stops{D2D1_GRADIENT_STOP{0.0F, tail},
                                                      D2D1_GRADIENT_STOP{1.0F, picture.head}};
        Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
        if (FAILED(target.CreateGradientStopCollection(
                stops.data(), static_cast<UINT32>(stops.size()), &collection)))
        {
            return nullptr;
        }
        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> brush;
        if (FAILED(target.CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(start, end),
                                                    collection.Get(), &brush)))
        {
            return nullptr;
        }
        return brush;
    }

    [[nodiscard]] static D2D1_POINT_2F middle(const core::CaretQuad &quad) noexcept
    {
        const auto &first = quad.corners.at(0);
        const auto &third = quad.corners.at(2);
        return D2D1::Point2F((first.x + third.x) / 2.0F, (first.y + third.y) / 2.0F);
    }
};
} // namespace nenenib::ui::win32
