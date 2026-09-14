#include "Direct2DRenderer.hpp"

#include "DevicePixels.hpp"

#include <array>
#include <cstddef>
#include <string>

namespace nenenib::ui::win32
{
namespace
{
// 採用案の寸法（docs/design/2026-09-15-look.md 第 2 節）。色は Palette 以外に持たない（ADR 0008）。
constexpr float tab_text_dips = 12.5F;
constexpr float ui_text_dips = 12.0F;
constexpr float code_text_dips = 13.5F;
constexpr std::int32_t tab_padding_left_dips = 14;
constexpr std::int32_t tab_padding_right_dips = 10;
constexpr std::int32_t tab_close_dips = 18;
constexpr std::int32_t body_top_dips = 12;
constexpr std::int32_t line_height_dips = 24;
constexpr std::int32_t gutter_width_dips = 56;
constexpr std::int32_t gutter_padding_dips = 16;
constexpr std::int32_t caret_width_dips = 2;
constexpr std::int32_t caret_height_dips = 20;
constexpr DWORD latency_timeout_milliseconds = 1000;
constexpr float full_channel = 255.0F;

// 「塗らない」を表す唯一の値。ui/win32 はこれ以外の色を Palette の外から持たない（ADR 0008）。
[[nodiscard]] constexpr D2D1_COLOR_F unpainted() noexcept
{
    return D2D1_COLOR_F{0.0F, 0.0F, 0.0F, 0.0F};
}

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
    return D2D1::ColorF(static_cast<float>(color.red) / full_channel,
                        static_cast<float>(color.green) / full_channel,
                        static_cast<float>(color.blue) / full_channel, 1.0F);
}

[[nodiscard]] D2D1_COLOR_F to_color(core::RgbaColor color) noexcept
{
    D2D1_COLOR_F opaque = to_color(color.color);
    opaque.a = static_cast<float>(color.alpha) / full_channel;
    return opaque;
}

[[nodiscard]] D2D1_RECT_F to_rect(const core::LayoutRect &area) noexcept
{
    return D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                       static_cast<float>(area.right), static_cast<float>(area.bottom));
}

[[nodiscard]] D2D1_POINT_2F centre_of(const core::LayoutRect &area) noexcept
{
    return D2D1::Point2F(static_cast<float>(area.left + area.right) / 2.0F,
                         static_cast<float>(area.top + area.bottom) / 2.0F);
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

std::expected<Direct2DRenderer, RenderFailure> Direct2DRenderer::create(HWND window,
                                                                        std::uint32_t dpi)
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
    return create_text_formats();
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
    // 座標は物理画素。寸法の拡大は core のレイアウト純関数が受け持つ（ADR 0008 の決定 5）。
    const auto reference = static_cast<float>(core::reference_dpi);
    context_->SetDpi(reference, reference);
    if (FAILED(context_->CreateSolidColorBrush(unpainted(), &brush_)))
    {
        return std::unexpected(RenderFailure::direct2d);
    }
    return {};
}

const wchar_t *Direct2DRenderer::family(const wchar_t *preferred, const wchar_t *fallback) const
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    if (FAILED(dwrite_->GetSystemFontCollection(collection.GetAddressOf(), FALSE)))
    {
        return fallback;
    }
    UINT32 index = 0;
    BOOL present = FALSE;
    if (FAILED(collection->FindFamilyName(preferred, &index, &present)))
    {
        return fallback;
    }
    return present != FALSE ? preferred : fallback;
}

HRESULT Direct2DRenderer::make_format(const wchar_t *face, float size_dips,
                                      DWRITE_FONT_WEIGHT weight, TextFormat &format)
{
    return dwrite_->CreateTextFormat(face, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                     DWRITE_FONT_STRETCH_NORMAL, scaled(size_dips), L"",
                                     format.ReleaseAndGetAddressOf());
}

void Direct2DRenderer::align_text_formats()
{
    const std::array<TextFormat, 6> every{tab_format_,  toggle_format_, status_format_,
                                          mode_format_, gutter_format_, code_format_};
    for (const auto &format : every)
    {
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    toggle_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    status_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    gutter_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
}

std::expected<void, RenderFailure> Direct2DRenderer::create_text_formats()
{
    Microsoft::WRL::ComPtr<IUnknown> unknown;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory7),
                                   unknown.GetAddressOf())) ||
        FAILED(unknown.As(&dwrite_)))
    {
        return std::unexpected(RenderFailure::directwrite);
    }
    // UI は Segoe UI Variable Text、本文は Cascadia Code。無い環境は順に落ちる（ADR 0008 の決定
    // 6）。
    const wchar_t *interface_face = family(L"Segoe UI Variable Text", L"Segoe UI");
    const wchar_t *code_face = family(L"Cascadia Code", L"Consolas");
    const HRESULT tab =
        make_format(interface_face, tab_text_dips, DWRITE_FONT_WEIGHT_NORMAL, tab_format_);
    const HRESULT toggle =
        make_format(interface_face, ui_text_dips, DWRITE_FONT_WEIGHT_SEMI_BOLD, toggle_format_);
    const HRESULT status =
        make_format(interface_face, ui_text_dips, DWRITE_FONT_WEIGHT_NORMAL, status_format_);
    const HRESULT mode =
        make_format(interface_face, ui_text_dips, DWRITE_FONT_WEIGHT_BOLD, mode_format_);
    const HRESULT gutter =
        make_format(code_face, ui_text_dips, DWRITE_FONT_WEIGHT_NORMAL, gutter_format_);
    const HRESULT code =
        make_format(code_face, code_text_dips, DWRITE_FONT_WEIGHT_NORMAL, code_format_);
    if (FAILED(tab) || FAILED(toggle) || FAILED(status) || FAILED(mode) || FAILED(gutter) ||
        FAILED(code))
    {
        return std::unexpected(RenderFailure::directwrite);
    }
    align_text_formats();
    return {};
}

float Direct2DRenderer::scaled(float dips) const noexcept
{
    return dips * static_cast<float>(dpi_) / static_cast<float>(core::reference_dpi);
}

void Direct2DRenderer::fill(const core::LayoutRect &area, core::RgbColor color)
{
    brush_->SetColor(to_color(color));
    context_->FillRectangle(to_rect(area), brush_.Get());
}

void Direct2DRenderer::fill_translucent(const core::LayoutRect &area, core::RgbaColor color)
{
    brush_->SetColor(to_color(color));
    context_->FillRectangle(to_rect(area), brush_.Get());
}

void Direct2DRenderer::fill_rounded(const core::LayoutRect &area, core::RgbColor color,
                                    float radius)
{
    brush_->SetColor(to_color(color));
    context_->FillRoundedRectangle(D2D1::RoundedRect(to_rect(area), radius, radius), brush_.Get());
}

void Direct2DRenderer::write(std::string_view text, IDWriteTextFormat *format,
                             const core::LayoutRect &area, core::RgbColor color)
{
    brush_->SetColor(to_color(color));
    const auto wide = widen(text);
    context_->DrawTextW(wide.c_str(), static_cast<UINT32>(wide.size()), format, to_rect(area),
                        brush_.Get());
}

void Direct2DRenderer::draw_cross(const core::LayoutRect &box, float half, float stroke)
{
    const auto centre = centre_of(box);
    context_->DrawLine(D2D1::Point2F(centre.x - half, centre.y - half),
                       D2D1::Point2F(centre.x + half, centre.y + half), brush_.Get(), stroke);
    context_->DrawLine(D2D1::Point2F(centre.x + half, centre.y - half),
                       D2D1::Point2F(centre.x - half, centre.y + half), brush_.Get(), stroke);
}

void Direct2DRenderer::draw_caption_glyphs(const core::TitleBarLayout &layout, core::RgbColor color)
{
    brush_->SetColor(to_color(color));
    const float stroke = scaled(1.0F);
    const float half = static_cast<float>(layout.glyph) / 2.0F;
    const auto minimize = centre_of(layout.minimize);
    context_->DrawLine(D2D1::Point2F(minimize.x - half, minimize.y),
                       D2D1::Point2F(minimize.x + half, minimize.y), brush_.Get(), stroke);
    const auto maximize = centre_of(layout.maximize);
    context_->DrawRectangle(
        D2D1::RectF(maximize.x - half, maximize.y - half, maximize.x + half, maximize.y + half),
        brush_.Get(), stroke);
    draw_cross(layout.close, half, stroke);
}

void Direct2DRenderer::draw_tab(const application::EditorFrame &frame,
                                const core::TitleBarLayout &layout)
{
    const auto tab = core::tab_rect(layout, 0);
    const auto radius = static_cast<float>(layout.corner_radius);
    fill_rounded(tab, frame.palette.tab_active, radius);
    fill(core::LayoutRect{tab.left, tab.bottom - layout.corner_radius, tab.right, tab.bottom},
         frame.palette.tab_active);
    fill(core::LayoutRect{tab.left, tab.bottom - layout.underline, tab.right, tab.bottom},
         frame.palette.accent);
    const auto close_width = core::to_pixels(tab_close_dips, dpi_);
    const auto right = tab.right - core::to_pixels(tab_padding_right_dips, dpi_);
    write(frame.tab_title.text(), tab_format_.Get(),
          core::LayoutRect{tab.left + core::to_pixels(tab_padding_left_dips, dpi_), tab.top,
                           right - close_width, tab.bottom},
          frame.palette.text);
    brush_->SetColor(to_color(frame.palette.muted));
    draw_cross(core::LayoutRect{right - close_width, tab.top, right, tab.bottom},
               static_cast<float>(close_width) / 4.0F, scaled(1.0F));
}

void Direct2DRenderer::draw_title_bar(const application::EditorFrame &frame,
                                      const core::TitleBarLayout &layout)
{
    // Mica が掛かった環境ではアルファ 0 のまま淡い面だけを載せ、掛からない環境は地の色で塗る。
    if (backdrop_ == TitleBarBackdrop::opaque)
    {
        fill(layout.band, frame.palette.background);
    }
    fill_translucent(layout.band, frame.palette.titlebar_tint);
    draw_tab(frame, layout);
    brush_->SetColor(to_color(frame.palette.muted));
    const auto plus = centre_of(layout.add_tab);
    const float half = static_cast<float>(layout.glyph) / 2.0F;
    const float stroke = scaled(1.0F);
    context_->DrawLine(D2D1::Point2F(plus.x - half, plus.y), D2D1::Point2F(plus.x + half, plus.y),
                       brush_.Get(), stroke);
    context_->DrawLine(D2D1::Point2F(plus.x, plus.y - half), D2D1::Point2F(plus.x, plus.y + half),
                       brush_.Get(), stroke);
    draw_caption_glyphs(layout, frame.palette.muted);
}

void Direct2DRenderer::draw_body(const application::EditorFrame &frame,
                                 const core::LayoutRect &area)
{
    fill(area, frame.palette.background);
    const auto gutter = core::to_pixels(gutter_width_dips, dpi_);
    const auto top = area.top + core::to_pixels(body_top_dips, dpi_);
    const core::LayoutRect line{area.left, top, area.right,
                                top + core::to_pixels(line_height_dips, dpi_)};
    fill(line, frame.palette.current_line);
    write("1", gutter_format_.Get(),
          core::LayoutRect{area.left, line.top,
                           area.left + gutter - core::to_pixels(gutter_padding_dips, dpi_),
                           line.bottom},
          frame.palette.text);
    const auto caret_height = core::to_pixels(caret_height_dips, dpi_);
    const auto caret_top = line.top + (core::height_of(line) - caret_height) / 2;
    fill(core::LayoutRect{area.left + gutter, caret_top,
                          area.left + gutter + core::to_pixels(caret_width_dips, dpi_),
                          caret_top + caret_height},
         frame.palette.accent);
    write(frame.text.text(), code_format_.Get(),
          core::LayoutRect{area.left + gutter, line.top, area.right, line.bottom},
          frame.palette.text);
}

void Direct2DRenderer::draw_toggle(const application::EditorFrame &frame,
                                   const core::StatusBarLayout &layout)
{
    fill_rounded(layout.toggle, frame.palette.toggle, static_cast<float>(layout.corner_radius));
    const bool vim = frame.mode == core::EditMode::vim;
    const auto &selected = vim ? layout.toggle_vim : layout.toggle_ordinary;
    fill_rounded(selected, frame.palette.accent, static_cast<float>(layout.segment_radius));
    write("通常", toggle_format_.Get(), layout.toggle_ordinary,
          vim ? frame.palette.muted : frame.palette.on_accent);
    write("Vim", toggle_format_.Get(), layout.toggle_vim,
          vim ? frame.palette.on_accent : frame.palette.muted);
}

void Direct2DRenderer::draw_status_bar(const application::EditorFrame &frame,
                                       const core::StatusBarLayout &layout)
{
    fill(layout.band, frame.palette.status);
    draw_toggle(frame, layout);
    write(frame.mode_label, mode_format_.Get(), layout.mode, frame.palette.text);
    for (std::size_t index = 0; index < core::status_item_count; ++index)
    {
        write(frame.status_items.at(index).text(), status_format_.Get(), layout.items.at(index),
              frame.palette.muted);
    }
}

std::expected<void, RenderFailure> Direct2DRenderer::draw(const application::EditorFrame &frame,
                                                          ID2D1Bitmap1 *surface)
{
    context_->SetTarget(surface);
    context_->BeginDraw();
    context_->Clear(unpainted());
    const auto size = context_->GetSize();
    const auto width = static_cast<std::int32_t>(size.width);
    const auto height = static_cast<std::int32_t>(size.height);
    const auto title = core::title_bar_layout(width, dpi_, 1);
    const auto status = core::status_bar_layout(width, height, dpi_);
    draw_title_bar(frame, title);
    draw_body(frame, core::LayoutRect{0, title.band.bottom, width, status.band.top});
    draw_status_bar(frame, status);
    const auto ended = context_->EndDraw();
    context_->SetTarget(nullptr);
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
    const auto reference = static_cast<float>(core::reference_dpi);
    const auto properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), reference,
        reference);
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

std::expected<void, RenderFailure> Direct2DRenderer::set_dpi(std::uint32_t dpi)
{
    dpi_ = dpi;
    // 文字の大きさは書式に焼かれているので、DPI が変わったら作り直す（ADR 0008 の決定 6）。
    return create_text_formats();
}

void Direct2DRenderer::set_backdrop(TitleBarBackdrop backdrop) noexcept
{
    backdrop_ = backdrop;
}
} // namespace nenenib::ui::win32
