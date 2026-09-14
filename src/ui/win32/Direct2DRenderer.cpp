#include "Direct2DRenderer.hpp"

#include "RgbColor.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::ui::win32
{
namespace
{
constexpr float points_to_dips = 96.0F / 72.0F;
constexpr float text_size_dips = 14.0F * points_to_dips;
constexpr float text_margin_dips = 16.0F;
constexpr DWORD latency_timeout_milliseconds = 1000;

[[nodiscard]] RenderFailure classify(HRESULT result) noexcept
{
    if (result == D2DERR_RECREATE_TARGET || result == DXGI_ERROR_DEVICE_REMOVED ||
        result == DXGI_ERROR_DEVICE_RESET)
    {
        return RenderFailure::device_lost;
    }
    return RenderFailure::direct2d;
}

[[nodiscard]] D2D1_COLOR_F to_color(core::RgbColor color) noexcept
{
    constexpr float full = 255.0F;
    return D2D1::ColorF(static_cast<float>(color.red) / full,
                        static_cast<float>(color.green) / full,
                        static_cast<float>(color.blue) / full, 1.0F);
}

// UTF-8 の表示値を DirectWrite の UTF-16 へ移す唯一の場所（CPP-014）。
[[nodiscard]] std::wstring widen(std::string_view text)
{
    const auto bytes = static_cast<int>(text.size());
    const int length =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), bytes, nullptr, 0);
    if (length <= 0)
    {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), bytes, wide.data(), length);
    return wide;
}

[[nodiscard]] UINT extent_of(LONG value) noexcept
{
    return value > 0 ? static_cast<UINT>(value) : 1U;
}
} // namespace

std::expected<Direct2DRenderer, RenderFailure> Direct2DRenderer::create(HWND window, float dpi)
{
    Direct2DRenderer renderer;
    renderer.dpi_ = dpi;
    const auto ready = renderer.initialize(window);
    if (!ready)
    {
        return std::unexpected(ready.error());
    }
    return renderer;
}

std::expected<void, RenderFailure> Direct2DRenderer::initialize(HWND window)
{
    const auto device = create_device();
    if (!device)
    {
        return device;
    }
    const auto chain = create_swap_chain(window);
    if (!chain)
    {
        return chain;
    }
    const auto bound = bind_composition(window);
    if (!bound)
    {
        return bound;
    }
    const auto context = create_context();
    if (!context)
    {
        return context;
    }
    return create_text_format();
}

std::expected<void, RenderFailure> Direct2DRenderer::create_device()
{
    constexpr UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    auto created = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0,
                                     D3D11_SDK_VERSION, &device_, nullptr, nullptr);
    if (FAILED(created))
    {
        // GPU が無い環境では WARP で描く（SPECIFICATION 第 9 節・Phase 0 の D1 は WARP で実測）。
        created = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0,
                                    D3D11_SDK_VERSION, &device_, nullptr, nullptr);
    }
    if (FAILED(created))
    {
        return std::unexpected(RenderFailure::device_creation);
    }
    if (FAILED(device_.As(&dxgi_device_)))
    {
        return std::unexpected(RenderFailure::device_creation);
    }
    return {};
}

std::expected<void, RenderFailure> Direct2DRenderer::create_swap_chain(HWND window)
{
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
    {
        return std::unexpected(RenderFailure::swap_chain);
    }
    RECT client{};
    GetClientRect(window, &client);
    DXGI_SWAP_CHAIN_DESC1 description{};
    description.Width = extent_of(client.right);
    description.Height = extent_of(client.bottom);
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 2;
    description.Scaling = DXGI_SCALING_STRETCH;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    description.Flags = static_cast<UINT>(DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
    Microsoft::WRL::ComPtr<IDXGISwapChain1> chain;
    if (FAILED(
            factory->CreateSwapChainForComposition(device_.Get(), &description, nullptr, &chain)) ||
        FAILED(chain.As(&swap_chain_)))
    {
        return std::unexpected(RenderFailure::swap_chain);
    }
    latency_.reset(swap_chain_->GetFrameLatencyWaitableObject());
    if (!latency_)
    {
        return std::unexpected(RenderFailure::swap_chain);
    }
    return {};
}

std::expected<void, RenderFailure> Direct2DRenderer::bind_composition(HWND window)
{
    if (FAILED(DCompositionCreateDevice(dxgi_device_.Get(), IID_PPV_ARGS(&composition_))))
    {
        return std::unexpected(RenderFailure::composition);
    }
    if (FAILED(composition_->CreateTargetForHwnd(window, TRUE, &target_)) ||
        FAILED(composition_->CreateVisual(&visual_)))
    {
        return std::unexpected(RenderFailure::composition);
    }
    if (FAILED(visual_->SetContent(swap_chain_.Get())) || FAILED(target_->SetRoot(visual_.Get())) ||
        FAILED(composition_->Commit()))
    {
        return std::unexpected(RenderFailure::composition);
    }
    return {};
}

std::expected<void, RenderFailure> Direct2DRenderer::create_context()
{
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.GetAddressOf())))
    {
        return std::unexpected(RenderFailure::direct2d);
    }
    Microsoft::WRL::ComPtr<ID2D1Device6> device;
    if (FAILED(factory_->CreateDevice(dxgi_device_.Get(), &device)))
    {
        return std::unexpected(RenderFailure::direct2d);
    }
    if (FAILED(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context_)))
    {
        return std::unexpected(RenderFailure::direct2d);
    }
    context_->SetDpi(dpi_, dpi_);
    return {};
}

HRESULT Direct2DRenderer::try_face(const wchar_t *face)
{
    return dwrite_->CreateTextFormat(face, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                     DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                     text_size_dips, L"", format_.ReleaseAndGetAddressOf());
}

std::expected<void, RenderFailure> Direct2DRenderer::create_text_format()
{
    Microsoft::WRL::ComPtr<IUnknown> unknown;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory7),
                                   unknown.GetAddressOf())) ||
        FAILED(unknown.As(&dwrite_)))
    {
        return std::unexpected(RenderFailure::directwrite);
    }
    // Segoe UI Variable は Windows 11 の標準書体。無い環境は Segoe UI へ落ちる（ADR 0007）。
    if (FAILED(try_face(L"Segoe UI Variable")) && FAILED(try_face(L"Segoe UI")))
    {
        return std::unexpected(RenderFailure::directwrite);
    }
    return {};
}

std::expected<void, RenderFailure> Direct2DRenderer::draw(const application::EditorFrame &frame,
                                                          ID2D1Bitmap1 *surface)
{
    context_->SetTarget(surface);
    context_->SetDpi(dpi_, dpi_);
    context_->BeginDraw();
    context_->Clear(to_color(frame.palette.background));
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    const auto ink = context_->CreateSolidColorBrush(to_color(frame.palette.text), &brush);
    if (SUCCEEDED(ink))
    {
        const auto line = widen(frame.text.text());
        const auto size = context_->GetSize();
        const auto bounds =
            D2D1::RectF(text_margin_dips, text_margin_dips, size.width - text_margin_dips,
                        size.height - text_margin_dips);
        context_->DrawTextW(line.c_str(), static_cast<UINT32>(line.size()), format_.Get(), bounds,
                            brush.Get());
    }
    const auto ended = context_->EndDraw();
    context_->SetTarget(nullptr);
    if (FAILED(ink))
    {
        return std::unexpected(RenderFailure::direct2d);
    }
    if (FAILED(ended))
    {
        return std::unexpected(classify(ended));
    }
    return {};
}

std::expected<void, RenderFailure> Direct2DRenderer::render(const application::EditorFrame &frame)
{
    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    const auto acquired = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&surface));
    if (FAILED(acquired))
    {
        return std::unexpected(classify(acquired));
    }
    const auto properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi_, dpi_);
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;
    const auto mapped = context_->CreateBitmapFromDxgiSurface(surface.Get(), &properties, &bitmap);
    if (FAILED(mapped))
    {
        return std::unexpected(classify(mapped));
    }
    const auto drawn = draw(frame, bitmap.Get());
    if (!drawn)
    {
        return drawn;
    }
    // flip model の待機可能オブジェクトで 1 フレーム分だけ待ってから提示する（Phase 0 D1）。
    if (WaitForSingleObjectEx(latency_.get(), latency_timeout_milliseconds, TRUE) != WAIT_OBJECT_0)
    {
        return std::unexpected(RenderFailure::swap_chain);
    }
    const auto presented = swap_chain_->Present(1, 0);
    if (FAILED(presented))
    {
        return std::unexpected(classify(presented));
    }
    return {};
}

std::expected<void, RenderFailure> Direct2DRenderer::resize(UINT width, UINT height)
{
    context_->SetTarget(nullptr);
    const auto resized = swap_chain_->ResizeBuffers(
        0, width == 0 ? 1U : width, height == 0 ? 1U : height, DXGI_FORMAT_UNKNOWN,
        static_cast<UINT>(DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));
    if (FAILED(resized))
    {
        return std::unexpected(classify(resized));
    }
    return {};
}

void Direct2DRenderer::set_dpi(float dpi) noexcept
{
    dpi_ = dpi;
}
} // namespace nenenib::ui::win32
