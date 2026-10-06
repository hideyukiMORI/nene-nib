#include "SoftwareTarget.hpp"

#include "DevicePixels.hpp"

#include <combaseapi.h>

namespace nenenib::tests::ui
{
namespace
{
constexpr UINT bitmap_width = 1300;
constexpr UINT bitmap_height = 80;
constexpr UINT bytes_per_pixel = 4;
} // namespace

SoftwareTarget::SoftwareTarget()
{
    Microsoft::WRL::ComPtr<ID2D1Factory> factory;
    Microsoft::WRL::ComPtr<IWICImagingFactory> imaging;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf())) ||
        FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&imaging))) ||
        FAILED(imaging->CreateBitmap(bitmap_width, bitmap_height, GUID_WICPixelFormat32bppPBGRA,
                                     WICBitmapCacheOnLoad, &bitmap_)))
    {
        return;
    }
    const auto reference = static_cast<float>(core::reference_dpi);
    const auto properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), reference,
        reference);
    if (FAILED(factory->CreateWicBitmapRenderTarget(bitmap_.Get(), properties, &target_)) ||
        FAILED(target_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brush_)))
    {
        target_.Reset();
        return;
    }
    target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
}

bool SoftwareTarget::ready() const noexcept
{
    return target_ != nullptr && brush_ != nullptr;
}

float SoftwareTarget::dpi() const
{
    float horizontal = 0;
    float vertical = 0;
    target_->GetDpi(&horizontal, &vertical);
    return horizontal == vertical ? horizontal : 0;
}

SoftwareTarget::Pixels SoftwareTarget::finish()
{
    Pixels pixels(std::size_t{bitmap_width} * bitmap_height * bytes_per_pixel);
    if (FAILED(target_->EndDraw()) ||
        FAILED(bitmap_->CopyPixels(nullptr, bitmap_width * bytes_per_pixel,
                                   static_cast<UINT>(pixels.size()), pixels.data())))
    {
        return {};
    }
    return pixels;
}

SoftwareTarget::Pixels SoftwareTarget::text_layout(IDWriteTextLayout *layout, D2D1_POINT_2F origin)
{
    target_->BeginDraw();
    target_->Clear(D2D1::ColorF(D2D1::ColorF::Black));
    target_->DrawTextLayout(origin, layout, brush_.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    return finish();
}

SoftwareTarget::Pixels
SoftwareTarget::glyph_runs(const std::vector<nenenib::ui::win32::BodyGlyphRun> &runs,
                           D2D1_POINT_2F origin, D2D1_RECT_F clip)
{
    target_->BeginDraw();
    target_->Clear(D2D1::ColorF(D2D1::ColorF::Black));
    target_->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_ALIASED);
    for (const auto &stored : runs)
    {
        const DWRITE_GLYPH_RUN run{stored.face.Get(),
                                   stored.em,
                                   static_cast<UINT32>(stored.indices.size()),
                                   stored.indices.data(),
                                   stored.advances.data(),
                                   stored.offsets.empty() ? nullptr : stored.offsets.data(),
                                   stored.sideways,
                                   stored.bidi};
        target_->DrawGlyphRun(D2D1::Point2F(origin.x + stored.origin.x, origin.y + stored.origin.y),
                              &run, brush_.Get(), stored.measuring);
    }
    target_->PopAxisAlignedClip();
    return finish();
}
} // namespace nenenib::tests::ui
