#include "Direct2DRenderer.hpp"
#include "BodyGlyphCollector.hpp"

#include "DevicePixels.hpp"
#include "DisplayLine.hpp"
#include "FontFallbackCache.hpp"
#include "InputLinePrompt.hpp"
#include "Milestone.hpp"
#include "PaletteLayout.hpp"
#include "PaletteOrigin.hpp"
#include "RgbaColor.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
namespace
{
// 採用案の寸法（docs/design/2026-09-15-look.md 第 2 節）。色は Palette 以外に持たない（ADR 0008）。
constexpr float tab_text_dips = 12.5F;
constexpr float ui_text_dips = 12.0F;
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
// 本文の Tab は空白この個数ぶんの tab stop に止まる。ADR 0034 の仮想桁（`tabstop` 固定 8）と
// 同じ値で、等幅フォントなら見える位置が Vim の桁と一致する（ADR 0045 の決定 1）。
constexpr float tab_stop_spaces = 8.0F;
// 空白 1 個を測る layout の最大幅。折り返さないので十分に大きければよい。
constexpr float space_probe_width = 1000.0F;
// モード表示の文字と `recording @a` のあいだ（ADR 0046 の決定 8）。
constexpr float recording_gap_dips = 12.0F;
// 一覧の行の題名と場所（`detail`）のあいだ（ADR 0057 の決定 7）。
constexpr float palette_detail_gap_dips = 12.0F;
// 一覧の行の右端の鍵の枠（docs/design/2026-10-06-guide.md の 5 節・ADR 0078 の決定 12）。
constexpr float palette_key_padding_dips = 9.0F;
constexpr float palette_key_height_dips = 20.0F;
constexpr float palette_key_radius_dips = 5.0F;
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

// 本文の桁（1 始まり）を描画用の行の桁へ（ADR 0040 の決定 3）。キャレット・選択・検索の当たり・
// IME の差し込み位置はすべてここを通してから UTF-16 の位置にする。範囲外は描画の末尾に畳む。
[[nodiscard]] core::Column displayed(const core::DisplayLine &line, core::Column column) noexcept
{
    const std::size_t index = column.value > 0 ? column.value - 1 : 0;
    return core::Column{core::display_position(line, index) + 1};
}

// 選択・検索の当たりの両端を描画用の行へ写す。変換は上の 1 本だけ（ADR 0040 の決定 3）。
[[nodiscard]] core::SelectionSpan displayed(const core::DisplayLine &line,
                                            const core::SelectionSpan &span) noexcept
{
    return core::SelectionSpan{span.presence, displayed(line, span.begin),
                               displayed(line, span.end)};
}

// 置き換えた文字（`is_replaced` の桁）の UTF-16 の範囲。IME の変換中の行では inserted に
// 変換中の文字列が差し込んであるので、その位置以降の範囲を長さぶん右へずらす（#152）。
// 本文だけの行は長さ 0 の inserted を渡す。差し込み位置は桁の境目なので範囲をまたがない。
[[nodiscard]] std::vector<DWRITE_TEXT_RANGE> replaced_ranges(const core::DisplayLine &line,
                                                             DWRITE_TEXT_RANGE inserted)
{
    std::vector<DWRITE_TEXT_RANGE> ranges;
    for (std::size_t column = 0; column + 1 < line.starts.size(); ++column)
    {
        if (!core::is_replaced(line, column))
        {
            continue;
        }
        const UINT32 from = utf16_offset(line.text, core::Column{line.starts.at(column) + 1});
        const UINT32 stop = utf16_offset(line.text, core::Column{line.starts.at(column + 1) + 1});
        const UINT32 shift = from >= inserted.startPosition ? inserted.length : 0U;
        ranges.push_back(DWRITE_TEXT_RANGE{from + shift, stop - from});
    }
    return ranges;
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

// 入力行のプロンプトの文字（ADR 0032 の決定 1）。選択肢が増えたらここで落ちる（CPP-002）。
[[nodiscard]] std::string prompt_text(core::InputLinePrompt prompt)
{
    switch (prompt)
    {
    case core::InputLinePrompt::ex:
        return ":";
    case core::InputLinePrompt::palette:
        return {};
    case core::InputLinePrompt::search_forward:
        return "/";
    case core::InputLinePrompt::search_backward:
        return "?";
    }
    std::unreachable();
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

HRESULT Direct2DRenderer::trim_by_character(IDWriteTextFormat *format)
{
    Microsoft::WRL::ComPtr<IDWriteInlineObject> ellipsis;
    const HRESULT made = dwrite_->CreateEllipsisTrimmingSign(format, ellipsis.GetAddressOf());
    if (FAILED(made))
    {
        return made;
    }
    const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    return format->SetTrimming(&trimming, ellipsis.Get());
}

std::expected<void, RenderFailure> Direct2DRenderer::create_text_formats()
{
    // 書式を解放する前に比較用の借用ポインタも消す（ADR 0070）。
    status_layouts_ = {};
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
    if (FAILED(trim_by_character(tab_format_.Get())))
    {
        return std::unexpected(RenderFailure::directwrite);
    }
    return create_body_formats(core::EditorSettings{formatted_size_, formatted_family_,
                                                    std::nullopt, core::GuideVisibility::shown});
}

std::expected<void, RenderFailure>
Direct2DRenderer::create_body_formats(const core::EditorSettings &settings)
{
    const std::wstring requested = core::to_utf16(settings.font_family.text()).value();
    const wchar_t *face = family(requested.c_str(), L"Consolas");
    const float ratio = core::font_size_ratio(settings.font_size);
    TextFormat code;
    TextFormat gutter;
    TextFormat key;
    const auto made_code = make_format(face, core::font_size_dips(settings.font_size),
                                       DWRITE_FONT_WEIGHT_NORMAL, code);
    const auto made_gutter =
        make_format(face, ui_text_dips * ratio, DWRITE_FONT_WEIGHT_NORMAL, gutter);
    const auto made_key = make_format(face, ui_text_dips * ratio, DWRITE_FONT_WEIGHT_NORMAL, key);
    if (FAILED(made_code) || FAILED(made_gutter) || FAILED(made_key))
    {
        return std::unexpected(RenderFailure::directwrite);
    }
    attach_font_fallback(code);
    const std::array<TextFormat, 3> every{code, gutter, key};
    for (const auto &format : every)
    {
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    gutter->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    set_tab_stops(code.Get());
    code_format_ = std::move(code);
    gutter_format_ = std::move(gutter);
    key_format_ = std::move(key);
    formatted_size_ = settings.font_size;
    formatted_family_ = settings.font_family;
    body_layouts_.clear();
    previous_body_layouts_.clear();
    return {};
}

void Direct2DRenderer::attach_font_fallback(const TextFormat &format)
{
    // 保持は速さのためだけにある。取り付けられなくても OS の既定の fallback で
    // 同じ字体を選べるので、窓を終わらせない（ADR 0077 の決定 5）。
    // 成功したときの呼び出しの列と順は ADR 0071 のまま。
    Microsoft::WRL::ComPtr<IDWriteFontFallback> system;
    Microsoft::WRL::ComPtr<IDWriteTextFormat2> version;
    if (FAILED(dwrite_->GetSystemFontFallback(&system)) || FAILED(format.As(&version)))
    {
        return;
    }
    static_cast<void>(
        version->SetFontFallback(Microsoft::WRL::Make<FontFallbackCache>(system).Get()));
}

void Direct2DRenderer::set_tab_stops(IDWriteTextFormat *format)
{
    // 同じ書式で空白 1 個を測る。`width` は末尾の空白を含まないので含む方を読む（ADR 0045）。
    TextLayout space;
    if (FAILED(dwrite_->CreateTextLayout(L" ", 1, format, space_probe_width, space_probe_width,
                                         space.GetAddressOf())))
    {
        return;
    }
    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(space->GetMetrics(&metrics)) || metrics.widthIncludingTrailingWhitespace <= 0.0F)
    {
        return;
    }
    // 失敗しても DirectWrite の既定の tab stop のまま描ける。描画は止めない（CPP-005）。
    static_cast<void>(
        format->SetIncrementalTabStop(metrics.widthIncludingTrailingWhitespace * tab_stop_spaces));
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

void Direct2DRenderer::write_status(std::string_view text, IDWriteTextFormat *format,
                                    const core::LayoutRect &area, core::RgbColor color)
{
    auto &entry = status_layouts_.at(status_layout_cursor_);
    ++status_layout_cursor_;
    const core::LayoutRect bounds{0, 0, core::width_of(area), core::height_of(area)};
    if (!entry.layout || entry.format != format || entry.area != bounds || entry.text != text)
    {
        auto made = text_layout(text, format, area);
        if (!made)
        {
            entry = {};
            return;
        }
        entry.text.assign(text);
        entry.area = bounds;
        entry.format = format;
        entry.layout = std::move(made);
    }
    brush_->SetColor(to_color(color));
    context_->DrawTextLayout(
        D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top)),
        entry.layout.Get(), brush_.Get());
}

void Direct2DRenderer::write_right(std::string_view text, IDWriteTextFormat *format,
                                   const core::LayoutRect &area, core::RgbColor color)
{
    if (core::width_of(area) <= 0 || core::height_of(area) <= 0)
    {
        return;
    }
    const auto shown = text_layout(text, format, area);
    if (!shown || FAILED(shown->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING)))
    {
        return;
    }
    brush_->SetColor(to_color(color));
    context_->PushAxisAlignedClip(to_rect(area), D2D1_ANTIALIAS_MODE_ALIASED);
    context_->DrawTextLayout(
        D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top)), shown.Get(),
        brush_.Get());
    context_->PopAxisAlignedClip();
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

void Direct2DRenderer::draw_tab_face(const core::TitleBarLayout &layout,
                                     const core::LayoutRect &tab, core::RgbColor color)
{
    // 上の角だけ丸め、下は帯の下端と面一にする（docs/design/2026-09-15-look.md 第 2 節）。
    fill_rounded(tab, color, static_cast<float>(layout.corner_radius));
    fill(core::LayoutRect{tab.left, tab.bottom - layout.corner_radius, tab.right, tab.bottom},
         color);
}

void Direct2DRenderer::draw_button_face(const application::EditorFrame &frame,
                                        const core::TitleBarLayout &layout,
                                        const core::LayoutRect &box, core::TitleBarTarget target)
{
    // ×・「∨」・「＋」にマウスを載せているときだけ toggle の面（ADR 0056 の決定 12）。
    if (layout.hovered == target)
    {
        fill_rounded(box, frame.palette.toggle, static_cast<float>(layout.button_radius));
    }
}

void Direct2DRenderer::draw_tab(const application::EditorFrame &frame,
                                const core::TitleBarLayout &layout, std::size_t index)
{
    const auto tab = core::tab_rect(layout, index);
    const bool active = index == layout.active;
    const bool lit = active || core::tab_hovered(layout, index);
    if (active)
    {
        draw_tab_face(layout, tab, frame.palette.tab_active);
        fill(core::LayoutRect{tab.left, tab.bottom - layout.underline, tab.right, tab.bottom},
             frame.palette.accent);
    }
    if (!active && lit)
    {
        draw_tab_face(layout, tab, frame.palette.tab_hover);
    }
    // × はアクティブなタブとマウスを載せたタブにだけある（D21）。題名はその左端まで。
    const auto close = core::tab_close_rect(layout, index);
    const auto right = close.has_value() ? close.value().left : tab.right - layout.tab_padding;
    const core::LayoutRect title{tab.left + layout.tab_padding, tab.top, right, tab.bottom};
    context_->PushAxisAlignedClip(to_rect(title), D2D1_ANTIALIAS_MODE_ALIASED);
    write(frame.tabs.at(index).title.text(), tab_format_.Get(), title,
          lit ? frame.palette.text : frame.palette.muted);
    context_->PopAxisAlignedClip();
    if (!close.has_value())
    {
        return;
    }
    draw_button_face(frame, layout, close.value(),
                     core::TitleBarTarget{core::TitleBarHit::tab_close, index});
    brush_->SetColor(to_color(frame.palette.muted));
    draw_cross(close.value(), static_cast<float>(layout.glyph) / 2.0F, scaled(1.0F));
}

void Direct2DRenderer::draw_tabs(const application::EditorFrame &frame,
                                 const core::TitleBarLayout &layout)
{
    // タブの領域の外（左へ送られた分と「∨」の下）は描かない（D20）。
    context_->PushAxisAlignedClip(to_rect(layout.viewport), D2D1_ANTIALIAS_MODE_ALIASED);
    const std::size_t count = std::min(frame.tabs.size(), layout.tab_count);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (core::tab_visible(layout, index))
        {
            draw_tab(frame, layout, index);
        }
    }
    context_->PopAxisAlignedClip();
}

void Direct2DRenderer::draw_add_tab(const application::EditorFrame &frame,
                                    const core::TitleBarLayout &layout)
{
    draw_button_face(frame, layout, layout.add_tab,
                     core::TitleBarTarget{core::TitleBarHit::add_tab, 0});
    brush_->SetColor(to_color(frame.palette.muted));
    const auto plus = centre_of(layout.add_tab);
    const float half = static_cast<float>(layout.glyph) / 2.0F;
    const float stroke = scaled(1.0F);
    context_->DrawLine(D2D1::Point2F(plus.x - half, plus.y), D2D1::Point2F(plus.x + half, plus.y),
                       brush_.Get(), stroke);
    context_->DrawLine(D2D1::Point2F(plus.x, plus.y - half), D2D1::Point2F(plus.x, plus.y + half),
                       brush_.Get(), stroke);
}

void Direct2DRenderer::draw_tab_list(const application::EditorFrame &frame,
                                     const core::TitleBarLayout &layout)
{
    // 「∨」はあふれているときだけある（D20）。押したときの一覧は #240。
    draw_button_face(frame, layout, layout.tab_list,
                     core::TitleBarTarget{core::TitleBarHit::tab_list, 0});
    brush_->SetColor(to_color(frame.palette.muted));
    const auto centre = centre_of(layout.tab_list);
    const float half = static_cast<float>(layout.glyph) / 2.0F;
    const float quarter = half / 2.0F;
    const float stroke = scaled(1.0F);
    context_->DrawLine(D2D1::Point2F(centre.x - half, centre.y - quarter),
                       D2D1::Point2F(centre.x, centre.y + quarter), brush_.Get(), stroke);
    context_->DrawLine(D2D1::Point2F(centre.x, centre.y + quarter),
                       D2D1::Point2F(centre.x + half, centre.y - quarter), brush_.Get(), stroke);
}

void Direct2DRenderer::draw_title_bar(const application::EditorFrame &frame,
                                      const core::TitleBarLayout &layout)
{
    // 帯は D16 で不透明。Mica は DWM 側に掛けたままだが、この面で隠れる（最初のフレームまでの
    // 面と非クライアントの明暗の判定に要るので外さない・ADR 0013 / ADR 0008 の決定 9）。
    fill(layout.band, frame.palette.title_bar);
    draw_tabs(frame, layout);
    if (layout.overflowing)
    {
        draw_tab_list(frame, layout);
    }
    draw_add_tab(frame, layout);
    draw_caption_glyphs(layout, frame.palette.muted);
}

Direct2DRenderer::TextLayout Direct2DRenderer::layout_of(std::string_view text,
                                                         const core::BodyLayout &body)
{
    const core::LayoutRect area{0, 0, core::width_of(body.content), body.line_height};
    const auto matches = [text, area](const BodyTextLayout &entry)
    {
        // 前の列から移した要素のlayoutは空。同じ空行として拾わない。
        return entry.layout && entry.area == area && entry.text == text;
    };
    const auto current = std::ranges::find_if(body_layouts_, matches);
    if (current != body_layouts_.end())
    {
        return current->layout;
    }
    const auto previous = std::ranges::find_if(previous_body_layouts_, matches);
    if (previous != previous_body_layouts_.end())
    {
        body_layouts_.push_back(std::move(*previous));
        return body_layouts_.back().layout;
    }
    auto made = text_layout(text, code_format_.Get(), area);
    if (!made)
    {
        return nullptr;
    }
    BodyTextLayout entry{std::string(text), area, std::move(made)};
    auto collector =
        Microsoft::WRL::Make<BodyGlyphCollector>(static_cast<float>(core::width_of(area)));
    // Draw は callback の失敗を返さないので、字形で描けるかは collector の take() が決める
    // （ADR 0073 の追記）。棄却なら字形を持たず DrawTextLayout で描く。
    auto taken = collector && SUCCEEDED(entry.layout->Draw(nullptr, collector.Get(), 0, 0))
                     ? collector->take()
                     : std::nullopt;
    entry.glyphs_ready = taken.has_value();
    entry.glyphs = std::move(taken).value_or(std::vector<BodyGlyphRun>{});
    body_layouts_.push_back(std::move(entry));
    return body_layouts_.back().layout;
}

// 保持した字形で描いた画素が DrawTextLayout と一致するのは、
// 次の 3 つがそろうときだけ（ADR 0073）。
// (1) 描画先の DPI が core::reference_dpi（96）で、collector の GetPixelsPerDip が 1。
// (2) 描画先の変換が恒等で、collector の GetCurrentTransform も恒等。
// (3) 行の原点 area.left / area.top が整数の画素。
// どれかを変えるときは字形の保持をやめるか、collector の答えを同じ値から出す。
// tests/ui が (1) と (2) の collector 側の値を確かめる。
void Direct2DRenderer::draw_body_text(IDWriteTextLayout *text, const core::LayoutRect &area)
{
    const auto entry = std::ranges::find_if(body_layouts_, [text](const BodyTextLayout &value)
                                            { return value.layout.Get() == text; });
    const auto origin = D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top));
    if (entry == body_layouts_.end() || !entry->glyphs_ready)
    {
        context_->DrawTextLayout(origin, text, brush_.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        return;
    }
    context_->PushAxisAlignedClip(D2D1::RectF(origin.x, origin.y, static_cast<float>(area.right),
                                              static_cast<float>(area.bottom)),
                                  D2D1_ANTIALIAS_MODE_ALIASED);
    for (const auto &stored : entry->glyphs)
    {
        const DWRITE_GLYPH_RUN run{stored.face.Get(),
                                   stored.em,
                                   static_cast<UINT32>(stored.indices.size()),
                                   stored.indices.data(),
                                   stored.advances.data(),
                                   stored.offsets.empty() ? nullptr : stored.offsets.data(),
                                   stored.sideways,
                                   stored.bidi};
        context_->DrawGlyphRun(
            D2D1::Point2F(origin.x + stored.origin.x, origin.y + stored.origin.y), &run,
            brush_.Get(), stored.measuring);
    }
    context_->PopAxisAlignedClip();
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

void Direct2DRenderer::outline_runs(IDWriteTextLayout *text, const core::LayoutRect &area,
                                    DWRITE_TEXT_RANGE range, float stroke)
{
    std::array<DWRITE_HIT_TEST_METRICS, selection_run_maximum> runs{};
    const std::size_t drawn = runs_of(text, area, range, std::span(runs));
    // 線は中心が座標に乗るので、半分だけ内側へ寄せて面からはみ出さないようにする。
    const float inset = stroke / 2.0F;
    for (std::size_t index = 0; index < drawn; ++index)
    {
        const auto &run = runs.at(index);
        context_->DrawRectangle(D2D1::RectF(run.left + inset, static_cast<float>(area.top) + inset,
                                            run.left + run.width - inset,
                                            static_cast<float>(area.bottom) - inset),
                                brush_.Get(), stroke);
    }
}

DWRITE_TEXT_RANGE Direct2DRenderer::range_of(const application::LineView &line,
                                             const core::SelectionSpan &span)
{
    const core::SelectionSpan shown = displayed(line.display, span);
    const UINT32 from = utf16_offset(line.display.text, shown.begin);
    const UINT32 stop = utf16_offset(line.display.text, shown.end);
    return DWRITE_TEXT_RANGE{from, stop - from};
}

void Direct2DRenderer::draw_line_matches(const application::EditorFrame &frame,
                                         IDWriteTextLayout *text, const core::LayoutRect &area,
                                         const application::LineView &line)
{
    brush_->SetColor(to_color(frame.palette.search));
    for (const auto &span : line.matches)
    {
        fill_runs(text, area, range_of(line, span));
    }
}

void Direct2DRenderer::draw_current_match(const application::EditorFrame &frame,
                                          IDWriteTextLayout *text, const core::LayoutRect &area,
                                          const application::LineView &line)
{
    if (!line.current_match.has_value())
    {
        return;
    }
    brush_->SetColor(to_color(frame.palette.accent));
    outline_runs(text, area, range_of(line, line.current_match.value()), scaled(1.0F));
}

void Direct2DRenderer::draw_line_selection(const application::EditorFrame &frame,
                                           IDWriteTextLayout *text, const core::LayoutRect &area,
                                           const application::LineView &line)
{
    const DWRITE_TEXT_RANGE range = range_of(line, line.selection);
    const UINT32 stop = range.startPosition + range.length;
    brush_->SetColor(to_color(frame.palette.selection));
    fill_runs(text, area, range);
    // 行末を越えたかは本文の桁で見る（描画の桁は末尾に畳まれている）。
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
                                        const core::DisplayLine &line)
{
    // 幅は桁の描画上の範囲。両端を displayed で写してから HitTestTextPosition で引く（ADR 0040 の
    // 決定 3）。行末では両端が同じ位置になり、既定の最小幅に畳む。
    const core::Column column = frame.caret.position.column;
    const float begin = caret_x(text, utf16_offset(line.text, displayed(line, column)));
    const float end =
        caret_x(text, utf16_offset(line.text, displayed(line, core::Column{column.value + 1})));
    const float left = static_cast<float>(area.left) + begin;
    const float width =
        std::max(end - begin, static_cast<float>(core::to_pixels(block_minimum_dips, dpi_)));
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
                                  const core::LayoutRect &area, const core::DisplayLine &line)
{
    switch (frame.caret.shape)
    {
    case core::CaretShape::bar:
        draw_bar_caret(frame, text, area,
                       utf16_offset(line.text, displayed(line, frame.caret.position.column)));
        return;
    case core::CaretShape::block:
        draw_block_caret(frame, text, area, line);
        return;
    }
    std::unreachable();
}

void Direct2DRenderer::draw_replaced(const application::EditorFrame &frame, IDWriteTextLayout *text,
                                     const core::LayoutRect &area,
                                     std::span<const DWRITE_TEXT_RANGE> ranges)
{
    for (const auto &range : ranges)
    {
        tint_runs(text, area, range, frame.palette.muted);
    }
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
                                    const core::LayoutRect &area, UINT32 base)
{
    // 本文の変換と面の入力行の変換は同時に値を持たない（ADR 0061 の決定 4）ので、在る方を引く。
    const auto &shown =
        frame.command_composition.has_value() ? frame.command_composition : frame.composition;
    if (!shown.has_value())
    {
        return;
    }
    // 変換中の文字列は UTF-16 の base に差し込んであるので、文節の位置はそこから数える。
    const auto &composition = shown.value();
    for (const auto &clause : composition.underlines)
    {
        const UINT32 from = base + utf16_at(composition.utf8, clause.range.begin.value);
        const UINT32 stop = base + utf16_at(composition.utf8, clause.range.end.value);
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
    // 差し込み位置も描画用の行の桁へ写してから探す（ADR 0040 の決定 3）。
    const std::size_t at =
        byte_of_column(line.display.text, displayed(line.display, frame.caret.position.column));
    std::string shown(line.display.text);
    shown.insert(at, composition.utf8);
    const auto text = layout_of(shown, body);
    if (!text)
    {
        return;
    }
    brush_->SetColor(to_color(frame.palette.text));
    draw_body_text(text.Get(), area);
    const UINT32 base = utf16_at(shown, at);
    // 置き換えた文字は変換中の行でも muted（ADR 0040 の決定 4）。IME の節とは重ならない。
    draw_replaced(
        frame, text.Get(), area,
        replaced_ranges(line.display, DWRITE_TEXT_RANGE{base, utf16_at(composition.utf8,
                                                                       composition.utf8.size())}));
    draw_clauses(frame, text.Get(), area, base);
    // 変換中のキャレットは GCS_CURSORPOS の位置のバー（ADR 0014 の決定 7）。
    draw_bar_caret(frame, text.Get(), area, utf16_at(shown, at + composition.cursor.value));
}

void Direct2DRenderer::draw_plain_line(const application::EditorFrame &frame,
                                       const core::BodyLayout &body, const core::LayoutRect &area,
                                       const application::LineView &line)
{
    const auto text = layout_of(line.display.text, body);
    if (!text)
    {
        return;
    }
    // 検索の当たり → 選択 → 本文 → 現在の当たりの枠 → キャレットの順（ADR 0037 の決定 5）。
    draw_line_matches(frame, text.Get(), area, line);
    if (line.selection.presence == core::SelectionPresence::present)
    {
        draw_line_selection(frame, text.Get(), area, line);
    }
    brush_->SetColor(to_color(frame.palette.text));
    draw_body_text(text.Get(), area);
    draw_replaced(frame, text.Get(), area, replaced_ranges(line.display, DWRITE_TEXT_RANGE{0, 0}));
    draw_current_match(frame, text.Get(), area, line);
    if (line.number == frame.caret.position.line && !frame.command_line.has_value())
    {
        draw_caret(frame, text.Get(), area, line.display);
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
    write_status("通常", toggle_format_.Get(), layout.toggle_ordinary,
                 vim ? frame.palette.muted : frame.palette.on_accent);
    write_status("Vim", toggle_format_.Get(), layout.toggle_vim,
                 vim ? frame.palette.on_accent : frame.palette.muted);
}

void Direct2DRenderer::draw_status_bar(const application::EditorFrame &frame,
                                       const core::StatusBarLayout &layout)
{
    status_layout_cursor_ = 0;
    fill(layout.band, frame.palette.status);
    draw_status_left(frame, layout);
    for (std::size_t index = 0; index < core::status_item_count; ++index)
    {
        write_status(frame.status_items.at(index).text(), status_format_.Get(),
                     layout.items.at(index), frame.palette.muted);
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
        write_status(frame.command_message.value().text(), command_format_.Get(), command.input,
                     frame.palette.text);
        context_->PopAxisAlignedClip();
        return;
    }
    draw_toggle(frame, layout);
    write_status(frame.mode_label, mode_format_.Get(), layout.mode, frame.palette.text);
    draw_recording(frame, layout);
}

// 録画中のマクロ（ADR 0046 の決定 8）。モード表示の文字の直後から右の項目の手前までに、
// Vim のコマンド行と同じ `recording @a` を muted で書く。色はトークンだけ（ADR 0008 決定 8）。
void Direct2DRenderer::draw_recording(const application::EditorFrame &frame,
                                      const core::StatusBarLayout &layout)
{
    if (!frame.recording.has_value())
    {
        return;
    }
    const auto label = text_layout(frame.mode_label, mode_format_.Get(), layout.mode);
    DWRITE_TEXT_METRICS metrics{};
    if (!label || FAILED(label->GetMetrics(&metrics)))
    {
        return;
    }
    const auto left = layout.mode.left +
                      static_cast<std::int32_t>(std::ceil(metrics.widthIncludingTrailingWhitespace +
                                                          scaled(recording_gap_dips)));
    const core::LayoutRect area{left, layout.band.top, layout.items.at(0).left, layout.band.bottom};
    if (core::width_of(area) <= 0)
    {
        return;
    }
    const std::string shown = std::string("recording @") + frame.recording.value();
    context_->PushAxisAlignedClip(to_rect(area), D2D1_ANTIALIAS_MODE_ALIASED);
    write_status(shown, command_format_.Get(), area, frame.palette.muted);
    context_->PopAxisAlignedClip();
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
    const std::string prefix = prompt_text(command.prompt);
    // 入力の文字のキャレットの位置（バイト）。変換中の文字列はここへ差し込む（ADR 0061 の決定 5）。
    const std::size_t at = prefix.size() + command.caret.value;
    auto shown = prefix + command.text;
    std::size_t caret = at;
    if (frame.command_composition.has_value())
    {
        const auto &composition = frame.command_composition.value();
        shown.insert(at, composition.utf8);
        caret = at + composition.cursor.value;
    }
    const auto text = text_layout(shown, command_format_.Get(), area);
    if (!text)
    {
        return;
    }
    float caret_x = 0.0F;
    float caret_y = 0.0F;
    DWRITE_HIT_TEST_METRICS metrics{};
    if (FAILED(
            text->HitTestTextPosition(utf16_at(shown, caret), FALSE, &caret_x, &caret_y, &metrics)))
    {
        return;
    }
    const float overflow =
        std::max(caret_x + scaled(3.0F) - static_cast<float>(core::width_of(area)), 0.0F);
    // 文節の下線は整数の原点から引くので、変換中だけは横ずらしを画素に揃える（下線と字を揃える）。
    const float offset = frame.command_composition.has_value() ? std::ceil(overflow) : overflow;
    const float origin = static_cast<float>(area.left) - offset;
    context_->PushAxisAlignedClip(to_rect(area), D2D1_ANTIALIAS_MODE_ALIASED);
    brush_->SetColor(to_color(frame.palette.text));
    context_->DrawTextLayout(D2D1::Point2F(origin, static_cast<float>(area.top)), text.Get(),
                             brush_.Get());
    // 文字の 1 行の上下（キャレットの棒と同じ）。原点は横ずらしの後の左端。
    const float line_top = static_cast<float>(area.top) + caret_y;
    auto line =
        D2D1::RectF(origin, line_top, static_cast<float>(area.right), line_top + metrics.height);
    if (frame.command_composition.has_value())
    {
        line = draw_command_clauses(frame, text.Get(), line, utf16_at(shown, at));
    }
    const auto caret_bar =
        D2D1::RectF(origin + caret_x, line.top, origin + caret_x + scaled(2.0F), line.bottom);
    brush_->SetColor(to_color(frame.palette.accent));
    context_->FillRectangle(caret_bar, brush_.Get());
    context_->PopAxisAlignedClip();
    // 候補窓はこの矩形の直下に出る（place_candidate_window・ADR 0061 の決定 5）。
    caret_rectangle_ =
        RECT{static_cast<LONG>(caret_bar.left), static_cast<LONG>(caret_bar.top),
             static_cast<LONG>(caret_bar.right), static_cast<LONG>(caret_bar.bottom)};
}

D2D1_RECT_F Direct2DRenderer::draw_command_clauses(const application::EditorFrame &frame,
                                                   IDWriteTextLayout *text, D2D1_RECT_F line,
                                                   UINT32 base)
{
    // 下線は本文の変換と同じ 1 本（ADR 0014 の決定 7）。本文と同じく 1 行の矩形を渡し、
    // 面と下線とキャレットの棒を同じ画素の上下に揃える。
    const float top = std::floor(line.top);
    const float bottom = std::ceil(line.bottom);
    // 他の文節の色は渡した上端から layout を描き直す（tint_runs）。上下の中央寄せの余白を
    // 上端の端数にして、描き直した字を入力行の字と同じ高さに置く。
    if (FAILED(text->SetMaxHeight((line.bottom - line.top) + (2.0F * (line.top - top)))))
    {
        return line;
    }
    draw_clauses(
        frame, text,
        core::LayoutRect{static_cast<std::int32_t>(line.left), static_cast<std::int32_t>(top),
                         static_cast<std::int32_t>(line.right), static_cast<std::int32_t>(bottom)},
        base);
    return D2D1::RectF(line.left, top, line.right, bottom);
}

void Direct2DRenderer::draw_completions(const application::EditorFrame &frame,
                                        const core::StatusBarLayout &status)
{
    if (!frame.command_line.has_value())
    {
        return;
    }
    const auto &command = frame.command_line.value();
    auto candidates = command.completions;
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
    const auto selected =
        frame.command_message.has_value() ? std::optional<std::size_t>{} : command.completion_index;
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
    // 名前・場所と右端の補足の欄は core の配置が決める（ADR 0060 の決定 9）。印の無い候補（Ex の
    // コマンド）は補足を持たず、名前の欄は行の内側の全部で今までと同じ。
    // frame は結果の窓だけを持つ（ADR 0062 の決定 2）。窓の外の位置は読まない（起きない）。
    if (index < palette.first || index - palette.first >= palette.rows.size())
    {
        return;
    }
    const auto &choice = palette.rows.at(index - palette.first);
    // 右端の欄は補足（印）か操作の鍵（ADR 0078 の決定 12）。両方を持つ行は無い。
    const auto label =
        core::palette_row_label(row, dpi_, choice.origin.has_value() || !choice.key.empty());
    context_->PushAxisAlignedClip(to_rect(label), D2D1_ANTIALIAS_MODE_ALIASED);
    write(choice.label.text(), command_format_.Get(), label, frame.palette.text);
    draw_palette_detail(frame, choice, label);
    context_->PopAxisAlignedClip();
    if (choice.origin.has_value())
    {
        write_right(core::palette_origin_label(choice.origin.value()), status_format_.Get(),
                    core::palette_row_note(row, dpi_), frame.palette.muted);
    }
    if (!choice.key.empty())
    {
        draw_palette_key(frame, choice.key, core::palette_row_note(row, dpi_));
    }
}

void Direct2DRenderer::draw_palette_key(const application::EditorFrame &frame, std::string_view key,
                                        const core::LayoutRect &note)
{
    // 文字の左右 9 DIP・高さ 20 DIP・角丸 5 DIP の枠を、右端を欄の右端に揃えて行の縦の中央に置く。
    // 幅は左寄せの書式の layout で測る（右寄せの書式で測らない・#258）。
    // 欄より広ければ欄の幅で切る。
    if (core::width_of(note) <= 0 || core::height_of(note) <= 0)
    {
        return;
    }
    const auto shown = text_layout(key, key_format_.Get(), note);
    DWRITE_TEXT_METRICS metrics{};
    if (!shown || FAILED(shown->GetMetrics(&metrics)))
    {
        return;
    }
    const float padding = scaled(palette_key_padding_dips);
    const float stroke = scaled(1.0F);
    const auto right = static_cast<float>(note.right);
    const float width =
        std::min(metrics.width + (padding * 2.0F), static_cast<float>(core::width_of(note)));
    const float middle = static_cast<float>(note.top + note.bottom) / 2.0F;
    const float half_height = scaled(palette_key_height_dips) / 2.0F;
    const D2D1_RECT_F box{right - width + (stroke / 2.0F), middle - half_height,
                          right - (stroke / 2.0F), middle + half_height};
    const float radius = scaled(palette_key_radius_dips);
    brush_->SetColor(to_color(frame.palette.background));
    context_->FillRoundedRectangle(D2D1::RoundedRect(box, radius, radius), brush_.Get());
    brush_->SetColor(to_color(frame.palette.panel_border));
    context_->DrawRoundedRectangle(D2D1::RoundedRect(box, radius, radius), brush_.Get(), stroke);
    brush_->SetColor(to_color(frame.palette.text));
    context_->PushAxisAlignedClip(box, D2D1_ANTIALIAS_MODE_ALIASED);
    context_->DrawTextLayout(D2D1::Point2F(right - width + padding, static_cast<float>(note.top)),
                             shown.Get(), brush_.Get());
    context_->PopAxisAlignedClip();
}

void Direct2DRenderer::draw_palette_detail(const application::EditorFrame &frame,
                                           const core::CommandChoice &choice,
                                           const core::LayoutRect &label)
{
    // 場所は題名の後ろに 12 DIP 空けて muted で書き、行の右端で文字単位に切る（ADR 0057 の
    // 決定 7）。題名が入りきらない行には書かない。detail の無い行（Ex の候補）は何もしない。
    if (!choice.detail.has_value())
    {
        return;
    }
    const auto title = text_layout(choice.label.text(), command_format_.Get(), label);
    DWRITE_TEXT_METRICS metrics{};
    if (!title || FAILED(title->GetMetrics(&metrics)))
    {
        return;
    }
    const auto left =
        label.left + static_cast<std::int32_t>(std::ceil(metrics.widthIncludingTrailingWhitespace +
                                                         scaled(palette_detail_gap_dips)));
    const core::LayoutRect place{left, label.top, label.right, label.bottom};
    if (core::width_of(place) <= 0)
    {
        return;
    }
    const auto detail = text_layout(choice.detail.value().text(), command_format_.Get(), place);
    if (!detail || FAILED(trim_by_character(detail.Get())))
    {
        return;
    }
    brush_->SetColor(to_color(frame.palette.muted));
    context_->DrawTextLayout(
        D2D1::Point2F(static_cast<float>(place.left), static_cast<float>(place.top)), detail.Get(),
        brush_.Get());
}

void Direct2DRenderer::draw_palette_choices(const application::EditorFrame &frame,
                                            const core::PaletteLayout &layout)
{
    if (!frame.command_palette.has_value())
    {
        return;
    }
    const auto &palette = frame.command_palette.value();
    if (palette.total == 0)
    {
        write("候補なし", mode_format_.Get(), core::palette_row(layout, 0), frame.palette.muted);
        return;
    }
    const auto start = core::palette_first_visible(layout, palette.selected);
    const auto count = std::min(layout.visible_rows, palette.total - start);
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
    const auto total = palette.total;
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
    // 記号の案内は入力より先に描く。出すかどうかは application が決め、ui は値があれば描く。
    if (frame.command_palette.has_value() && frame.command_palette.value().hint.has_value())
    {
        write_right(frame.command_palette.value().hint.value().text(), status_format_.Get(),
                    core::palette_input_hint(layout), frame.palette.muted);
    }
    draw_command(frame, layout.input);
    context_->PushAxisAlignedClip(to_rect(layout.rows), D2D1_ANTIALIAS_MODE_ALIASED);
    draw_palette_choices(frame, layout);
    context_->PopAxisAlignedClip();
    draw_palette_footer(frame, layout.footer);
}

std::expected<void, RenderFailure> Direct2DRenderer::draw(const application::EditorFrame &frame,
                                                          ID2D1Bitmap1 *surface)
{
    previous_body_layouts_.swap(body_layouts_);
    body_layouts_.clear();
    context_->SetTarget(surface);
    context_->BeginDraw();
    context_->Clear(unpainted());
    const auto size = context_->GetSize();
    const auto width = static_cast<std::int32_t>(size.width);
    const auto height = static_cast<std::int32_t>(size.height);
    const auto title = core::title_bar_layout(application::title_bar_input(frame, width, dpi_));
    const auto status = core::status_bar_layout(width, height, dpi_);
    draw_title_bar(frame, title);
    draw_body(frame, core::body_layout(width, height, dpi_, frame.settings.font_size));
    draw_status_bar(frame, status);
    if (frame.command_palette.has_value())
    {
        draw_palette(
            frame, core::palette_layout(width, height, dpi_, frame.command_palette.value().total));
    }
    previous_body_layouts_.clear();
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

core::Column Direct2DRenderer::column_at(const application::LineView &line,
                                         const core::BodyLayout &body, std::int32_t x)
{
    const auto layout = layout_of(line.display.text, body);
    BOOL trailing = FALSE;
    BOOL inside = FALSE;
    DWRITE_HIT_TEST_METRICS metrics{};
    if (!layout || FAILED(layout->HitTestPoint(static_cast<float>(x - body.content.left), 0.0F,
                                               &trailing, &inside, &metrics)))
    {
        return core::Column{1};
    }
    const UINT32 position = metrics.textPosition + (trailing != FALSE ? metrics.length : 0U);
    const std::size_t shown = code_points_before(line.display.text, position);
    return core::Column{core::source_column(line.display, shown) + 1};
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
