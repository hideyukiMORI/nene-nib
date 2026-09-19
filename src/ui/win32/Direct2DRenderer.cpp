#include "Direct2DRenderer.hpp"

#include "DevicePixels.hpp"
#include "Milestone.hpp"
#include "RgbaColor.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
// 採用案の寸法（docs/design/2026-09-15-look.md 第 2 節）。色は Palette 以外に持たない（ADR 0008）。
constexpr float tab_text_dips = 12.5F;
constexpr float ui_text_dips = 12.0F;
constexpr std::int32_t tab_padding_left_dips = 14;
constexpr std::int32_t tab_padding_right_dips = 10;
constexpr std::int32_t tab_close_dips = 18;
constexpr std::int32_t gutter_padding_dips = 16;
constexpr std::int32_t caret_inset_dips = 2;
constexpr std::int32_t newline_mark_dips = 6;
constexpr std::int32_t block_minimum_dips = 7;
constexpr std::int32_t block_radius_dips = 1;
// IME の文節の下線（採用案 D15・ADR 0014 の決定 7）。注目文節は太く、他は細い。
constexpr std::int32_t target_underline_dips = 2;
constexpr std::int32_t other_underline_dips = 1;
// 折り返さない 1 行なので当たりの矩形は 1 つで足りる。多い分は行の帯からはみ出すだけ。
constexpr std::size_t selection_run_maximum = 8;
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

// UTF-8 の表示値を DirectWrite の UTF-16 へ。変換そのものは core::to_utf16 ただ 1 本
// （CPP-014 / Issue #13）。表示値は検証済みなので、空になるのは本当に空のときだけ。
[[nodiscard]] std::wstring widen(std::string_view text)
{
    return core::to_utf16(text).value_or(std::wstring{});
}

[[nodiscard]] UINT extent_of(LONG value) noexcept
{
    return value > 0 ? static_cast<UINT>(value) : 1U;
}

// 桁（code point・1 始まり）を本文のバイト位置へ。行の中の走査はここ 1 本（CPP-014）。
[[nodiscard]] std::size_t byte_of_column(std::string_view text, core::Column column)
{
    std::size_t byte = 0;
    for (std::size_t step = 1; step < column.value && byte < text.size(); ++step)
    {
        byte = core::next_code_point(text, core::Offset{byte}).value;
    }
    return byte;
}

// バイト位置を DirectWrite の UTF-16 の位置へ。変換はここでだけ起きる（CPP-014）。
[[nodiscard]] UINT32 utf16_at(std::string_view text, std::size_t byte)
{
    return static_cast<UINT32>(widen(text.substr(0, std::min(byte, text.size()))).size());
}

[[nodiscard]] UINT32 utf16_offset(std::string_view text, core::Column column)
{
    return utf16_at(text, byte_of_column(text, column));
}

// UTF-16 の位置より前に code point がいくつあるか。utf16_offset の逆（CPP-014）。
[[nodiscard]] std::size_t code_points_before(std::string_view text, UINT32 position)
{
    std::size_t byte = 0;
    std::size_t units = 0;
    std::size_t points = 0;
    while (byte < text.size() && units < position)
    {
        const std::size_t next = core::next_code_point(text, core::Offset{byte}).value;
        // UTF-8 で 4 バイトの code point だけが UTF-16 でサロゲートペアの 2 単位になる。
        units += next - byte >= 4 ? 2U : 1U;
        byte = next;
        ++points;
    }
    return points;
}

[[nodiscard]] float caret_x(IDWriteTextLayout *text, UINT32 position) noexcept
{
    DWRITE_HIT_TEST_METRICS metrics{};
    float x = 0.0F;
    float y = 0.0F;
    if (FAILED(text->HitTestTextPosition(position, FALSE, &x, &y, &metrics)))
    {
        return 0.0F;
    }
    return x;
}
} // namespace

std::expected<Direct2DRenderer, RenderFailure>
Direct2DRenderer::create(HWND window, std::uint32_t dpi, application::TimingPort &timing)
{
    Direct2DRenderer renderer;
    renderer.dpi_ = dpi;
    const auto ready = renderer.initialize(window, timing);
    if (!ready)
    {
        return std::unexpected(ready.error());
    }
    return renderer;
}

// 段ごとに節目を打つ（Issue #19）。device lost で作り直すと同じ節目が再び積まれるが、
// 計測器は最初の出現だけを読むので起動の内訳は濁らない。
std::expected<void, RenderFailure> Direct2DRenderer::initialize(HWND window,
                                                                application::TimingPort &timing)
{
    const auto device = create_device();
    if (!device)
    {
        return device;
    }
    timing.mark(core::Milestone::device_created);
    const auto chain = create_swap_chain(window);
    if (!chain)
    {
        return chain;
    }
    timing.mark(core::Milestone::swap_chain_created);
    const auto bound = bind_composition(window);
    if (!bound)
    {
        return bound;
    }
    timing.mark(core::Milestone::composition_bound);
    const auto context = create_context();
    if (!context)
    {
        return context;
    }
    timing.mark(core::Milestone::context_created);
    const auto formats = create_text_formats();
    if (!formats)
    {
        return formats;
    }
    timing.mark(core::Milestone::text_formats_created);
    return {};
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
    const std::array<TextFormat, 5> every{tab_format_, toggle_format_, status_format_, mode_format_,
                                          command_format_};
    for (const auto &format : every)
    {
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    toggle_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    status_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
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
    const HRESULT tab =
        make_format(interface_face, tab_text_dips, DWRITE_FONT_WEIGHT_NORMAL, tab_format_);
    const HRESULT toggle =
        make_format(interface_face, ui_text_dips, DWRITE_FONT_WEIGHT_SEMI_BOLD, toggle_format_);
    const HRESULT status =
        make_format(interface_face, ui_text_dips, DWRITE_FONT_WEIGHT_NORMAL, status_format_);
    const HRESULT mode =
        make_format(interface_face, ui_text_dips, DWRITE_FONT_WEIGHT_BOLD, mode_format_);
    const HRESULT command = make_format(family(L"Cascadia Code", L"Consolas"), ui_text_dips,
                                        DWRITE_FONT_WEIGHT_NORMAL, command_format_);
    if (FAILED(tab) || FAILED(toggle) || FAILED(status) || FAILED(mode) || FAILED(command))
    {
        return std::unexpected(RenderFailure::directwrite);
    }
    align_text_formats();
    return create_body_formats(
        core::EditorSettings{formatted_size_, formatted_family_, std::nullopt});
}

std::expected<void, RenderFailure>
Direct2DRenderer::create_body_formats(const core::EditorSettings &settings)
{
    const std::wstring requested = core::to_utf16(settings.font_family.text()).value();
    const wchar_t *face = family(requested.c_str(), L"Consolas");
    const float ratio = core::font_size_ratio(settings.font_size);
    TextFormat code;
    TextFormat gutter;
    const auto made_code = make_format(face, core::font_size_dips(settings.font_size),
                                       DWRITE_FONT_WEIGHT_NORMAL, code);
    const auto made_gutter =
        make_format(face, ui_text_dips * ratio, DWRITE_FONT_WEIGHT_NORMAL, gutter);
    if (FAILED(made_code) || FAILED(made_gutter))
    {
        return std::unexpected(RenderFailure::directwrite);
    }
    const std::array<TextFormat, 2> every{code, gutter};
    for (const auto &format : every)
    {
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    gutter->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    code_format_ = std::move(code);
    gutter_format_ = std::move(gutter);
    formatted_size_ = settings.font_size;
    formatted_family_ = settings.font_family;
    return {};
}

std::expected<void, RenderFailure> Direct2DRenderer::set_font(const core::EditorSettings &settings)
{
    if (formatted_size_.points() == settings.font_size.points() &&
        formatted_family_.text() == settings.font_family.text())
    {
        return {};
    }
    return create_body_formats(settings);
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
    write(frame.document.title.text(), tab_format_.Get(),
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
    // 帯は D16 で不透明。Mica は DWM 側に掛けたままだが、この面で隠れる（最初のフレームまでの
    // 面と非クライアントの明暗の判定に要るので外さない・ADR 0013 / ADR 0008 の決定 9）。
    fill(layout.band, frame.palette.title_bar);
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

Direct2DRenderer::TextLayout Direct2DRenderer::layout_of(std::string_view text,
                                                         const core::BodyLayout &body)
{
    return text_layout(text, code_format_.Get(),
                       core::LayoutRect{0, 0, core::width_of(body.content), body.line_height});
}

Direct2DRenderer::TextLayout Direct2DRenderer::text_layout(std::string_view text,
                                                           IDWriteTextFormat *format,
                                                           const core::LayoutRect &area)
{
    const auto wide = widen(text);
    TextLayout made;
    // 折り返さず幅で切る（ADR 0009 の決定 6）。書式の側に NO_WRAP を立ててある。
    if (FAILED(dwrite_->CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()), format,
                                         static_cast<float>(core::width_of(area)),
                                         static_cast<float>(core::height_of(area)), &made)))
    {
        return nullptr;
    }
    return made;
}

std::size_t Direct2DRenderer::runs_of(IDWriteTextLayout *text, const core::LayoutRect &area,
                                      DWRITE_TEXT_RANGE range,
                                      std::span<DWRITE_HIT_TEST_METRICS> runs)
{
    UINT32 count = 0;
    if (FAILED(text->HitTestTextRange(range.startPosition, range.length,
                                      static_cast<float>(area.left), static_cast<float>(area.top),
                                      runs.data(), static_cast<UINT32>(runs.size()), &count)))
    {
        return 0;
    }
    return std::min(static_cast<std::size_t>(count), runs.size());
}

void Direct2DRenderer::fill_runs(IDWriteTextLayout *text, const core::LayoutRect &area,
                                 DWRITE_TEXT_RANGE range)
{
    std::array<DWRITE_HIT_TEST_METRICS, selection_run_maximum> runs{};
    const std::size_t drawn = runs_of(text, area, range, std::span(runs));
    const auto top = static_cast<float>(area.top);
    for (std::size_t index = 0; index < drawn; ++index)
    {
        const auto &run = runs.at(index);
        context_->FillRectangle(
            D2D1::RectF(run.left, top, run.left + run.width, static_cast<float>(area.bottom)),
            brush_.Get());
    }
}

void Direct2DRenderer::underline_runs(IDWriteTextLayout *text, const core::LayoutRect &area,
                                      DWRITE_TEXT_RANGE range, std::int32_t thickness)
{
    std::array<DWRITE_HIT_TEST_METRICS, selection_run_maximum> runs{};
    const std::size_t drawn = runs_of(text, area, range, std::span(runs));
    const auto bottom = static_cast<float>(area.bottom - core::to_pixels(caret_inset_dips, dpi_));
    for (std::size_t index = 0; index < drawn; ++index)
    {
        const auto &run = runs.at(index);
        context_->FillRectangle(D2D1::RectF(run.left, bottom - static_cast<float>(thickness),
                                            run.left + run.width, bottom),
                                brush_.Get());
    }
}

void Direct2DRenderer::tint_runs(IDWriteTextLayout *text, const core::LayoutRect &area,
                                 DWRITE_TEXT_RANGE range, core::RgbColor color)
{
    std::array<DWRITE_HIT_TEST_METRICS, selection_run_maximum> runs{};
    const std::size_t drawn = runs_of(text, area, range, std::span(runs));
    const auto origin = D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top));
    brush_->SetColor(to_color(color));
    for (std::size_t index = 0; index < drawn; ++index)
    {
        const auto &run = runs.at(index);
        context_->PushAxisAlignedClip(D2D1::RectF(run.left, static_cast<float>(area.top),
                                                  run.left + run.width,
                                                  static_cast<float>(area.bottom)),
                                      D2D1_ANTIALIAS_MODE_ALIASED);
        context_->DrawTextLayout(origin, text, brush_.Get());
        context_->PopAxisAlignedClip();
    }
}

void Direct2DRenderer::draw_line_selection(const application::EditorFrame &frame,
                                           IDWriteTextLayout *text, const core::LayoutRect &area,
                                           const application::LineView &line)
{
    const UINT32 from = utf16_offset(line.text, line.selection.begin);
    const UINT32 stop = utf16_offset(line.text, line.selection.end);
    brush_->SetColor(to_color(frame.palette.selection));
    fill_runs(text, area, DWRITE_TEXT_RANGE{from, stop - from});
    if (line.selection.end.value <= core::code_point_count(line.text) + 1)
    {
        return;
    }
    // 行をまたぐ選択は、改行のぶんだけ行末からはみ出して塗る（採用案 第 1 節）。
    const float x = static_cast<float>(area.left) + caret_x(text, stop);
    const auto mark = static_cast<float>(core::to_pixels(newline_mark_dips, dpi_));
    context_->FillRectangle(
        D2D1::RectF(x, static_cast<float>(area.top), x + mark, static_cast<float>(area.bottom)),
        brush_.Get());
}

void Direct2DRenderer::draw_bar_caret(const application::EditorFrame &frame,
                                      IDWriteTextLayout *text, const core::LayoutRect &area,
                                      UINT32 position)
{
    const auto inset = core::to_pixels(caret_inset_dips, dpi_);
    const auto left = area.left + static_cast<std::int32_t>(caret_x(text, position));
    const core::LayoutRect bar{left, area.top + inset, left + caret_width_, area.bottom - inset};
    fill(bar, frame.palette.accent);
    caret_rectangle_ = RECT{bar.left, bar.top, bar.right, bar.bottom};
}

void Direct2DRenderer::draw_block_caret(const application::EditorFrame &frame,
                                        IDWriteTextLayout *text, const core::LayoutRect &area,
                                        UINT32 position)
{
    DWRITE_HIT_TEST_METRICS metrics{};
    float x = 0.0F;
    float y = 0.0F;
    if (FAILED(text->HitTestTextPosition(position, FALSE, &x, &y, &metrics)))
    {
        return;
    }
    const float left = static_cast<float>(area.left) + metrics.left;
    const float width =
        std::max(metrics.width, static_cast<float>(core::to_pixels(block_minimum_dips, dpi_)));
    const auto block = D2D1::RectF(left, static_cast<float>(area.top), left + width,
                                   static_cast<float>(area.bottom));
    caret_rectangle_ =
        RECT{static_cast<LONG>(left), area.top, static_cast<LONG>(left + width), area.bottom};
    const auto radius = static_cast<float>(core::to_pixels(block_radius_dips, dpi_));
    brush_->SetColor(to_color(frame.palette.accent));
    context_->FillRoundedRectangle(D2D1::RoundedRect(block, radius, radius), brush_.Get());
    // 覆った 1 文字だけを on_accent で描き直す。切り抜きの中に行の layout をもう一度通す。
    context_->PushAxisAlignedClip(block, D2D1_ANTIALIAS_MODE_ALIASED);
    brush_->SetColor(to_color(frame.palette.on_accent));
    context_->DrawTextLayout(
        D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top)), text,
        brush_.Get());
    context_->PopAxisAlignedClip();
}

void Direct2DRenderer::draw_caret(const application::EditorFrame &frame, IDWriteTextLayout *text,
                                  const core::LayoutRect &area, std::string_view line)
{
    const UINT32 position = utf16_offset(line, frame.caret.position.column);
    switch (frame.caret.shape)
    {
    case core::CaretShape::bar:
        draw_bar_caret(frame, text, area, position);
        return;
    case core::CaretShape::block:
        draw_block_caret(frame, text, area, position);
        return;
    }
    std::unreachable();
}

void Direct2DRenderer::draw_target_clause(const application::EditorFrame &frame,
                                          IDWriteTextLayout *text, const core::LayoutRect &area,
                                          DWRITE_TEXT_RANGE range)
{
    brush_->SetColor(to_color(frame.palette.selection));
    fill_runs(text, area, range);
    brush_->SetColor(to_color(frame.palette.accent));
    underline_runs(text, area, range, core::to_pixels(target_underline_dips, dpi_));
}

void Direct2DRenderer::draw_other_clause(const application::EditorFrame &frame,
                                         IDWriteTextLayout *text, const core::LayoutRect &area,
                                         DWRITE_TEXT_RANGE range)
{
    tint_runs(text, area, range, frame.palette.ime);
    brush_->SetColor(to_color(frame.palette.ime));
    underline_runs(text, area, range, core::to_pixels(other_underline_dips, dpi_));
}

void Direct2DRenderer::draw_clauses(const application::EditorFrame &frame, IDWriteTextLayout *text,
                                    const core::LayoutRect &area, std::string_view shown)
{
    if (!frame.composition.has_value())
    {
        return;
    }
    // 変換中の文字列はキャレットの桁に差し込んであるので、その手前までが行の元の字である。
    const std::size_t at = byte_of_column(shown, frame.caret.position.column);
    for (const auto &clause : frame.composition.value().underlines)
    {
        const UINT32 from = utf16_at(shown, at + clause.range.begin.value);
        const UINT32 stop = utf16_at(shown, at + clause.range.end.value);
        const DWRITE_TEXT_RANGE range{from, stop - from};
        switch (clause.emphasis)
        {
        case core::ClauseEmphasis::target:
            draw_target_clause(frame, text, area, range);
            break;
        case core::ClauseEmphasis::other:
            draw_other_clause(frame, text, area, range);
            break;
        }
    }
}

void Direct2DRenderer::draw_composed_line(const application::EditorFrame &frame,
                                          const core::BodyLayout &body,
                                          const core::LayoutRect &area,
                                          const application::LineView &line)
{
    if (!frame.composition.has_value())
    {
        return;
    }
    const auto &composition = frame.composition.value();
    const std::size_t at = byte_of_column(line.text, frame.caret.position.column);
    std::string shown(line.text);
    shown.insert(at, composition.utf8);
    const auto text = layout_of(shown, body);
    if (!text)
    {
        return;
    }
    brush_->SetColor(to_color(frame.palette.text));
    context_->DrawTextLayout(
        D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top)), text.Get(),
        brush_.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    draw_clauses(frame, text.Get(), area, shown);
    // 変換中のキャレットは GCS_CURSORPOS の位置のバー（ADR 0014 の決定 7）。
    draw_bar_caret(frame, text.Get(), area, utf16_at(shown, at + composition.cursor.value));
}

void Direct2DRenderer::draw_plain_line(const application::EditorFrame &frame,
                                       const core::BodyLayout &body, const core::LayoutRect &area,
                                       const application::LineView &line)
{
    const auto text = layout_of(line.text, body);
    if (!text)
    {
        return;
    }
    if (line.selection.presence == core::SelectionPresence::present)
    {
        draw_line_selection(frame, text.Get(), area, line);
    }
    brush_->SetColor(to_color(frame.palette.text));
    context_->DrawTextLayout(
        D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top)), text.Get(),
        brush_.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    if (line.number == frame.caret.position.line && !frame.command_line.has_value())
    {
        draw_caret(frame, text.Get(), area, line.text);
    }
}

void Direct2DRenderer::draw_line(const application::EditorFrame &frame,
                                 const core::BodyLayout &body, std::size_t index)
{
    const auto &line = frame.lines.at(index);
    const auto row = core::body_line_rect(body, index);
    if (row.bottom > body.band.bottom)
    {
        return;
    }
    const bool on_caret_line = line.number == frame.caret.position.line;
    if (on_caret_line)
    {
        fill(row, frame.palette.current_line);
    }
    write(std::to_string(line.number.value), gutter_format_.Get(),
          core::LayoutRect{
              body.gutter.left, row.top,
              body.gutter.right -
                  core::to_pixels(static_cast<std::int32_t>(
                                      static_cast<float>(gutter_padding_dips) *
                                          core::font_size_ratio(frame.settings.font_size) +
                                      0.5F),
                                  dpi_),
              row.bottom},
          frame.palette.gutter);
    const core::LayoutRect area{body.content.left, row.top, body.content.right, row.bottom};
    // 変換中の文字列はキャレットの行にだけ差し込まれる（ADR 0014 の決定 2）。
    if (on_caret_line && frame.composition.has_value())
    {
        draw_composed_line(frame, body, area, line);
        return;
    }
    draw_plain_line(frame, body, area, line);
}

void Direct2DRenderer::draw_body(const application::EditorFrame &frame,
                                 const core::BodyLayout &body)
{
    fill(body.band, frame.palette.background);
    caret_width_ = body.caret_width;
    for (std::size_t index = 0; index < frame.lines.size(); ++index)
    {
        draw_line(frame, body, index);
    }
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
    draw_status_left(frame, layout);
    for (std::size_t index = 0; index < core::status_item_count; ++index)
    {
        write(frame.status_items.at(index).text(), status_format_.Get(), layout.items.at(index),
              frame.palette.muted);
    }
}

void Direct2DRenderer::draw_status_left(const application::EditorFrame &frame,
                                        const core::StatusBarLayout &layout)
{
    const auto command = core::command_layout(layout, dpi_, 0);
    if (frame.command_line.has_value() && !frame.command_palette.has_value())
    {
        draw_command(frame, command.input);
        draw_completions(frame, layout);
        return;
    }
    if (frame.command_message.has_value() && !frame.command_palette.has_value())
    {
        context_->PushAxisAlignedClip(to_rect(command.input), D2D1_ANTIALIAS_MODE_ALIASED);
        write(frame.command_message.value().text(), command_format_.Get(), command.input,
              frame.palette.text);
        context_->PopAxisAlignedClip();
        return;
    }
    draw_toggle(frame, layout);
    write(frame.mode_label, mode_format_.Get(), layout.mode, frame.palette.text);
}

void Direct2DRenderer::draw_command(const application::EditorFrame &frame,
                                    const core::LayoutRect &area)
{
    if (core::width_of(area) <= 0)
    {
        return;
    }
    if (!frame.command_line.has_value())
    {
        return;
    }
    const auto &command = frame.command_line.value();
    const std::string prefix = frame.command_palette.has_value() ? "" : ":";
    const auto shown = prefix + std::string(command.text());
    const auto text = text_layout(shown, command_format_.Get(), area);
    if (!text)
    {
        return;
    }
    float caret_x = 0.0F;
    float caret_y = 0.0F;
    DWRITE_HIT_TEST_METRICS metrics{};
    if (FAILED(text->HitTestTextPosition(utf16_at(shown, command.caret().value + prefix.size()),
                                         FALSE, &caret_x, &caret_y, &metrics)))
    {
        return;
    }
    const auto offset =
        std::max(caret_x + scaled(3.0F) - static_cast<float>(core::width_of(area)), 0.0F);
    const float origin = static_cast<float>(area.left) - offset;
    context_->PushAxisAlignedClip(to_rect(area), D2D1_ANTIALIAS_MODE_ALIASED);
    brush_->SetColor(to_color(frame.palette.text));
    context_->DrawTextLayout(D2D1::Point2F(origin, static_cast<float>(area.top)), text.Get(),
                             brush_.Get());
    brush_->SetColor(to_color(frame.palette.accent));
    context_->FillRectangle(D2D1::RectF(origin + caret_x, static_cast<float>(area.top) + caret_y,
                                        origin + caret_x + scaled(2.0F),
                                        static_cast<float>(area.top) + caret_y + metrics.height),
                            brush_.Get());
    context_->PopAxisAlignedClip();
}

void Direct2DRenderer::draw_completions(const application::EditorFrame &frame,
                                        const core::StatusBarLayout &status)
{
    if (!frame.command_line.has_value())
    {
        return;
    }
    const auto &command = frame.command_line.value();
    auto candidates = command.completions();
    if (frame.command_message.has_value())
    {
        candidates = {std::string(frame.command_message.value().text())};
    }
    const auto layout = core::command_layout(status, dpi_, candidates.size());
    if (layout.visible_rows == 0 || core::width_of(layout.panel) <= 0)
    {
        return;
    }
    fill(layout.panel, frame.palette.panel);
    const auto selected = frame.command_message.has_value() ? std::optional<std::size_t>{}
                                                            : command.completion_index();
    const auto start =
        std::max(selected.value_or(0) + 1, layout.visible_rows) - layout.visible_rows;
    context_->PushAxisAlignedClip(to_rect(layout.panel), D2D1_ANTIALIAS_MODE_ALIASED);
    for (std::size_t index = 0; index < layout.visible_rows; ++index)
    {
        const auto top = layout.panel.top + static_cast<std::int32_t>(index) * layout.row_height;
        const core::LayoutRect row{layout.panel.left, top, layout.panel.right,
                                   top + layout.row_height};
        const bool active = selected == start + index;
        if (active)
        {
            fill(row, frame.palette.accent);
        }
        write(candidates.at(start + index), command_format_.Get(), row,
              active ? frame.palette.on_accent : frame.palette.text);
    }
    context_->PopAxisAlignedClip();
    brush_->SetColor(to_color(frame.palette.panel_border));
    context_->DrawRectangle(to_rect(layout.panel), brush_.Get(), scaled(1.0F));
}

void Direct2DRenderer::draw_palette_choice(const application::EditorFrame &frame,
                                           const core::LayoutRect &row, std::size_t index)
{
    if (!frame.command_palette.has_value())
    {
        return;
    }
    const auto &palette = frame.command_palette.value();
    const auto inset = core::to_pixels(8, dpi_);
    if (palette.selected == index)
    {
        brush_->SetColor(to_color(frame.palette.selection));
        const core::LayoutRect selected{row.left + inset, row.top, row.right - inset, row.bottom};
        context_->FillRoundedRectangle(
            D2D1::RoundedRect(to_rect(selected), scaled(6.0F), scaled(6.0F)), brush_.Get());
    }
    const core::LayoutRect label{row.left + inset * 2, row.top, row.right - inset * 2, row.bottom};
    context_->PushAxisAlignedClip(to_rect(label), D2D1_ANTIALIAS_MODE_ALIASED);
    write(palette.choices.at(index).label.text(), command_format_.Get(), label, frame.palette.text);
    context_->PopAxisAlignedClip();
}

void Direct2DRenderer::draw_palette_choices(const application::EditorFrame &frame,
                                            const core::PaletteLayout &layout)
{
    if (!frame.command_palette.has_value())
    {
        return;
    }
    const auto &palette = frame.command_palette.value();
    if (palette.choices.empty())
    {
        write("候補なし", mode_format_.Get(), core::palette_row(layout, 0), frame.palette.muted);
        return;
    }
    const auto start = core::palette_first_visible(layout, palette.selected);
    const auto count = std::min(layout.visible_rows, palette.choices.size() - start);
    for (std::size_t index = 0; index < count; ++index)
    {
        draw_palette_choice(frame, core::palette_row(layout, index), start + index);
    }
}

void Direct2DRenderer::draw_palette_footer(const application::EditorFrame &frame,
                                           const core::LayoutRect &area)
{
    if (!frame.command_palette.has_value())
    {
        return;
    }
    const auto &palette = frame.command_palette.value();
    const auto total = palette.choices.size();
    const auto position = total == 0 ? 0 : palette.selected + 1;
    const auto split = std::max(area.left, area.right - core::to_pixels(64, dpi_));
    const core::LayoutRect hint_area{area.left, area.top, split, area.bottom};
    const auto hint = frame.command_message.has_value()
                          ? frame.command_message.value().text()
                          : std::string_view("↑↓ 選択   Enter 決定   Esc 閉じる");
    context_->PushAxisAlignedClip(to_rect(hint_area), D2D1_ANTIALIAS_MODE_ALIASED);
    write(hint, mode_format_.Get(), hint_area, frame.palette.muted);
    context_->PopAxisAlignedClip();
    const core::LayoutRect count_area{split, area.top, area.right, area.bottom};
    context_->PushAxisAlignedClip(to_rect(count_area), D2D1_ANTIALIAS_MODE_ALIASED);
    write(std::to_string(position) + " / " + std::to_string(total), status_format_.Get(),
          count_area, frame.palette.muted);
    context_->PopAxisAlignedClip();
}

void Direct2DRenderer::draw_palette(const application::EditorFrame &frame,
                                    const core::PaletteLayout &layout)
{
    if (core::width_of(layout.input) <= 0 || core::height_of(layout.input) <= 0)
    {
        return;
    }
    fill_rounded(layout.panel, frame.palette.panel, scaled(10.0F));
    brush_->SetColor(to_color(frame.palette.panel_border));
    context_->DrawRoundedRectangle(
        D2D1::RoundedRect(to_rect(layout.panel), scaled(10.0F), scaled(10.0F)), brush_.Get(),
        scaled(1.0F));
    context_->DrawLine(D2D1::Point2F(static_cast<float>(layout.panel.left),
                                     static_cast<float>(layout.input.bottom)),
                       D2D1::Point2F(static_cast<float>(layout.panel.right),
                                     static_cast<float>(layout.input.bottom)),
                       brush_.Get(), scaled(1.0F));
    draw_command(frame, layout.input);
    context_->PushAxisAlignedClip(to_rect(layout.rows), D2D1_ANTIALIAS_MODE_ALIASED);
    draw_palette_choices(frame, layout);
    context_->PopAxisAlignedClip();
    draw_palette_footer(frame, layout.footer);
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
    draw_body(frame, core::body_layout(width, height, dpi_, frame.settings.font_size));
    draw_status_bar(frame, status);
    if (frame.command_palette.has_value())
    {
        draw_palette(frame, core::palette_layout(width, height, dpi_,
                                                 frame.command_palette.value().choices.size()));
    }
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
    const auto formatted = set_font(frame.settings);
    if (!formatted)
    {
        return formatted;
    }
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

core::Column Direct2DRenderer::column_at(std::string_view text, const core::BodyLayout &body,
                                         std::int32_t x)
{
    const auto layout = layout_of(text, body);
    BOOL trailing = FALSE;
    BOOL inside = FALSE;
    DWRITE_HIT_TEST_METRICS metrics{};
    if (!layout || FAILED(layout->HitTestPoint(static_cast<float>(x - body.content.left), 0.0F,
                                               &trailing, &inside, &metrics)))
    {
        return core::Column{1};
    }
    const UINT32 position = metrics.textPosition + (trailing != FALSE ? metrics.length : 0U);
    return core::Column{code_points_before(text, position) + 1};
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

RECT Direct2DRenderer::caret_rectangle() const noexcept
{
    return caret_rectangle_;
}
} // namespace nenenib::ui::win32
