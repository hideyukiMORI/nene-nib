#include "Direct2DRenderer.hpp"
#include "TextLayoutPoint.hpp"

#include "BodyColors.hpp"
#include "ColoredRun.hpp"
#include "ComposedLine.hpp"
#include "DevicePixels.hpp"
#include "DisplayFormLabel.hpp"
#include "DisplayLine.hpp"
#include "EditMode.hpp"
#include "EditorSettings.hpp"
#include "GuideContext.hpp"
#include "GuideKeyWidth.hpp"
#include "GuideRequest.hpp"
#include "GutterLabels.hpp"
#include "InputLinePrompt.hpp"
#include "Milestone.hpp"
#include "PaletteLayout.hpp"
#include "PaletteOrigin.hpp"
#include "PaneLayouts.hpp"
#include "PaneRole.hpp"
#include "PreviewPlacement.hpp"
#include "PreviewTextStyle.hpp"
#include "ReplacedTints.hpp"
#include "RgbaColor.hpp"
#include "SyntaxColor.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::ui::win32
{
namespace
{
// 採用案の寸法（docs/design/2026-09-15-look.md 第 2 節）。色は Palette 以外に持たない（ADR 0008）。
constexpr float tab_text_dips = 12.5F;
constexpr float ui_text_dips = 12.0F;
constexpr std::int32_t gutter_padding_dips = 16;
// text の表示の巨大な行の印 `»`（ADR 0139 の決定 2）。
constexpr std::string_view giant_mark = "\xC2\xBB";
constexpr std::int32_t caret_inset_dips = 2;
constexpr std::int32_t newline_mark_dips = 6;
constexpr std::int32_t block_minimum_dips = 7;
constexpr std::int32_t block_radius_dips = 1;
// IME の文節の下線（採用案 D15・ADR 0014 の決定 7）。注目文節は太く、他は細い。
constexpr std::int32_t target_underline_dips = 2;
constexpr std::int32_t other_underline_dips = 1;

// モード表示の文字と `recording @a` のあいだ（ADR 0046 の決定 8）。
constexpr float recording_gap_dips = 12.0F;
// 一覧の行の題名と場所（`detail`）のあいだ（ADR 0057 の決定 7）。
constexpr float palette_detail_gap_dips = 12.0F;
// 一覧の行の右端の鍵の枠（docs/design/2026-10-06-guide.md の 5 節・ADR 0078 の決定 12）。
constexpr float palette_key_padding_dips = 9.0F;
constexpr float palette_key_height_dips = 20.0F;
constexpr float palette_key_radius_dips = 5.0F;
// 空の文書の案内の鍵の枠の左右の余白（docs/design/2026-10-06-guide.md の 2 節）。箱の幅を測る
// guide_key_width と描く draw_body_guide が同じ値を使う（#1122）。
constexpr float guide_key_padding_dips = 9.0F;
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

// 本文のキャレットを描かないとき。入力行が開いているか、エクスプローラーの面に焦点があるとき
// （採用案の表「焦点」・Ctrl+P の面と同じ）。
[[nodiscard]] bool body_caret_hidden(const application::EditorFrame &frame) noexcept
{
    return frame.command_line.has_value() ||
           (frame.explorer.has_value() && frame.explorer.value().focused);
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

// バイト位置を DirectWrite の UTF-16 の位置へ。単位数は core の正典で測る（CPP-014）。
[[nodiscard]] UINT32 utf16_at(std::string_view text, std::size_t byte)
{
    return static_cast<UINT32>(
        core::utf16_length(text.substr(0, std::min(byte, text.size()))).value_or(0));
}

// 本文の桁（1 始まり）を描画用の行の桁へ（ADR 0040 の決定 3）。キャレット・選択・検索の当たり・
// IME の差し込み位置はすべてここを通してから UTF-16 の位置にする。範囲外は描画の末尾に畳む。
[[nodiscard]] core::Column displayed(const core::DisplayLine &line, core::Column column) noexcept
{
    const std::size_t index = column.value > 0 ? column.value - 1 : 0;
    return core::Column{core::display_position(line, index) + 1};
}

// 選択・検索の当たりの両端を描画用の行へ写す。変換は上の 1 本だけ（ADR 0040 の決定 3）。
// 巨大な行の窓では、本文の桁を窓の中の桁に写す（ADR 0139）。窓の前は窓の始まりに畳む。
[[nodiscard]] core::Column windowed_column(const application::CaretView &caret,
                                           const application::LineView &shown) noexcept
{
    const auto source = caret.position.column.value;
    return core::Column{source - std::min(shown.window.code_point, source - 1)};
}

// もう 1 つの枠のキャレットの印の形（採用案の案 A）。通常モードは縦棒、Vim はモードを問わず
// 中空のブロック。操作中の枠の形（CaretView::shape）は Vim のモードで変わるので使わない。
[[nodiscard]] core::CaretShape passive_shape(core::EditMode mode) noexcept
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return core::CaretShape::bar;
    case core::EditMode::vim:
        return core::CaretShape::block;
    }
    std::unreachable();
}

[[nodiscard]] core::SelectionSpan displayed(const core::DisplayLine &line,
                                            const core::SelectionSpan &span) noexcept
{
    return core::SelectionSpan{span.presence, displayed(line, span.begin),
                               displayed(line, span.end)};
}

[[nodiscard]] std::int32_t row_pixel(const core::BodyLayout &body, std::int64_t row)
{
    const auto value =
        static_cast<double>(body.content.top) + static_cast<double>(row) * body.line_height;
    return static_cast<std::int32_t>(
        std::clamp(value, static_cast<double>(std::numeric_limits<std::int32_t>::min()) / 2.0,
                   static_cast<double>(std::numeric_limits<std::int32_t>::max()) / 2.0));
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

// 行番号の欄の中の文字の枠。右寄せの番号は右に余白を置き、左寄せ（relativenumber のキャレットの
// 行・#1041）は同じ幅の余白を左にも置く（左右対称）。
[[nodiscard]] core::LayoutRect
gutter_number_area(core::LayoutRect cell, core::GutterAlignment alignment, std::int32_t padding)
{
    if (alignment == core::GutterAlignment::leading)
    {
        cell.left += padding;
    }
    cell.right -= padding;
    return cell;
}
} // namespace

Direct2DRenderer::Direct2DRenderer(BodyTextLayouts &text_layouts) : body_text_(text_layouts) {}

std::expected<Direct2DRenderer, RenderFailure>
Direct2DRenderer::create(HWND window, std::uint32_t dpi, application::TimingPort &timing,
                         RendererSources sources)
{
    Direct2DRenderer renderer(sources.text_layouts);
    renderer.dpi_ = dpi;
    return started(std::move(renderer), window, timing, sources.warmup);
}

std::expected<Direct2DRenderer, RenderFailure>
Direct2DRenderer::create_provisional(HWND cover, std::uint32_t dpi, application::TimingPort &timing,
                                     RendererSources sources)
{
    Direct2DRenderer renderer(sources.text_layouts);
    renderer.dpi_ = dpi;
    renderer.provisional_ = true;
    return started(std::move(renderer), cover, timing, sources.warmup);
}

std::expected<Direct2DRenderer, RenderFailure>
Direct2DRenderer::started(Direct2DRenderer renderer, HWND window, application::TimingPort &timing,
                          DeviceWarmup &warmup)
{
    const auto ready = renderer.initialize(window, timing, warmup);
    if (!ready)
    {
        return std::unexpected(ready.error());
    }
    return renderer;
}

// 段ごとに節目を打つ（Issue #19）。device lost で作り直すと同じ節目が再び積まれるが、
// 計測器は最初の出現だけを読むので起動の内訳は濁らない。
std::expected<void, RenderFailure>
Direct2DRenderer::initialize(HWND window, application::TimingPort &timing, DeviceWarmup &warmup)
{
    auto device = create_device(warmup);
    if (!device)
    {
        return std::unexpected(device.error());
    }
    timing.mark(core::Milestone::device_created);
    const auto chain = create_swap_chain(window, std::move(device).value());
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

std::expected<Microsoft::WRL::ComPtr<IDXGIFactory2>, RenderFailure>
Direct2DRenderer::create_device(DeviceWarmup &warmup)
{
    // worker が先に作った device を 1 回だけ受け取る。device lost の作り直しでは箱は空なので、
    // 今日どおりここで作る（ADR 0131 の決定 3）。仮の renderer は WARP だけを受け取り、
    // 自分では作らない（ADR 0132 の決定 2・factory は空なので swap chain が作る）。
    auto warmed = provisional_ ? warmup.take_provisional() : warmup.take();
    if (warmed.has_value())
    {
        device_ = std::move(warmed.value().device);
        if (FAILED(device_.As(&dxgi_device_)))
        {
            return std::unexpected(RenderFailure::device_creation);
        }
        return std::move(warmed.value().factory);
    }
    if (provisional_)
    {
        return std::unexpected(RenderFailure::device_creation);
    }
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

// factory は箱から受け取ったものを使い、無ければここで作る（ADR 0131 の決定 3）。
std::expected<void, RenderFailure>
Direct2DRenderer::create_swap_chain(HWND window, Microsoft::WRL::ComPtr<IDXGIFactory2> factory)
{
    if (!factory && FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
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
    // 待てる frame を 1 つに固定する（ADR 0007 の追補・#888）。ResizeBuffers の後も保たれる。
    if (FAILED(swap_chain_->SetMaximumFrameLatency(1)))
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
    palette_layouts_.clear();
    tab_layouts_.clear();
    explorer_layouts_.clear();
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
    const auto guide = create_guide_formats(interface_face);
    if (!guide)
    {
        return guide;
    }
    if (FAILED(trim_by_character(tab_format_.Get())))
    {
        return std::unexpected(RenderFailure::directwrite);
    }
    return create_body_formats(core::EditorSettings{formatted_size_, formatted_family_,
                                                    std::nullopt, core::GuideVisibility::shown});
}

std::expected<void, RenderFailure> Direct2DRenderer::create_guide_formats(const wchar_t *face)
{
    // 鍵の箱の幅は書式で測ったものなので、書式と一緒に捨てる（#1122）。
    measured_guide_keys_.reset();
    measured_guide_key_width_ = 0;
    const auto key = make_format(family(L"Cascadia Code", L"Consolas"), 12.5F,
                                 DWRITE_FONT_WEIGHT_NORMAL, guide_key_format_);
    const auto label = make_format(face, 13.5F, DWRITE_FONT_WEIGHT_NORMAL, guide_label_format_);
    const auto note = make_format(face, 11.5F, DWRITE_FONT_WEIGHT_NORMAL, guide_note_format_);
    const auto status =
        make_format(face, ui_text_dips, DWRITE_FONT_WEIGHT_NORMAL, guide_status_format_);
    if (FAILED(key) || FAILED(label) || FAILED(note) || FAILED(status))
    {
        return std::unexpected(RenderFailure::directwrite);
    }
    const std::array<TextFormat, 4> every{guide_key_format_, guide_label_format_,
                                          guide_note_format_, guide_status_format_};
    for (const auto &format : every)
    {
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    guide_note_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    return {};
}

std::expected<void, RenderFailure>
Direct2DRenderer::create_body_formats(const core::EditorSettings &settings)
{
    // 書式の寿命をキーに含める。生成が失敗しても古い行番号資源は持ち越さない。
    gutter_layouts_.clear();
    palette_layouts_.clear();
    tab_layouts_.clear();
    explorer_layouts_.clear();
    const std::wstring requested = core::to_utf16(settings.font_family.text()).value();
    const wchar_t *face = family(requested.c_str(), L"Consolas");
    const float ratio = core::font_size_ratio(settings.font_size);
    TextFormat gutter;
    TextFormat key;
    TextFormat explorer;

    const auto made_gutter =
        make_format(face, ui_text_dips * ratio, DWRITE_FONT_WEIGHT_NORMAL, gutter);
    // エクスプローラーの面の名前は UI の字体で、本文の大きさに追従する（行の高さが本文と同じ・
    // ADR 0162 の決定 2）。長い名前は文字単位の省略記号で切る（タブの題名と同じ規則）。
    const auto made_explorer =
        make_format(family(L"Segoe UI Variable Text", L"Segoe UI"), ui_text_dips * ratio,
                    DWRITE_FONT_WEIGHT_NORMAL, explorer);
    // 面の行の名前・説明（`command_format_`）と右端の欄（112 DIP）は本文の大きさに追従しないので、
    // 鍵も倍率を掛けない 12 DIP に固定する（ADR 0078 の決定 12・#312）。
    const auto made_key = make_format(face, ui_text_dips, DWRITE_FONT_WEIGHT_NORMAL, key);
    if (FAILED(made_gutter) || FAILED(made_key) || FAILED(made_explorer) ||
        FAILED(trim_by_character(explorer.Get())))
    {
        return std::unexpected(RenderFailure::directwrite);
    }
    const std::array<TextFormat, 3> every{gutter, key, explorer};
    for (const auto &format : every)
    {
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    gutter->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    gutter_format_ = std::move(gutter);
    key_format_ = std::move(key);
    explorer_format_ = std::move(explorer);
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

void Direct2DRenderer::write_status(std::string_view text, IDWriteTextFormat *format,
                                    const core::LayoutRect &area, core::RgbColor color)
{
    draw_status_text(status_text_layout(text, format, area), area, color);
}

const StatusTextLayout &Direct2DRenderer::status_text_layout(std::string_view text,
                                                             IDWriteTextFormat *format,
                                                             const core::LayoutRect &area)
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
            return entry;
        }
        entry.text.assign(text);
        entry.area = bounds;
        entry.format = format;
        entry.layout = std::move(made);
    }
    return entry;
}

void Direct2DRenderer::draw_status_text(const StatusTextLayout &entry, const core::LayoutRect &area,
                                        core::RgbColor color)
{
    if (!entry.layout)
    {
        return;
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

// 書式は右寄せ 1 つ。左寄せ（relativenumber のキャレットの行・#1041）は作った layout だけを
// 寄せ直し、寄せも保持の鍵に入れる。
void Direct2DRenderer::write_gutter(std::string_view text, core::GutterAlignment alignment,
                                    const core::LayoutRect &area, core::RgbColor color)
{
    const auto retained = gutter_layouts_.lookup(text, area, alignment);
    auto layout = retained.value_or(nullptr);
    if (!layout)
    {
        layout = text_layout(text, gutter_format_.Get(), area);
        if (layout && alignment == core::GutterAlignment::leading &&
            FAILED(layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING)))
        {
            layout = nullptr;
        }
        gutter_layouts_.retain(text, area, alignment, layout);
    }
    if (!layout)
    {
        return;
    }
    brush_->SetColor(to_color(color));
    context_->DrawTextLayout(
        D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top)), layout.Get(),
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
    write_tab_title(frame.tabs.at(index).title.text(), title,
                    lit ? frame.palette.text : frame.palette.muted);
    if (!close.has_value())
    {
        return;
    }
    draw_button_face(frame, layout, close.value(),
                     core::TitleBarTarget{core::TitleBarHit::tab_close, index});
    brush_->SetColor(to_color(frame.palette.muted));
    draw_cross(close.value(), static_cast<float>(layout.glyph) / 2.0F, scaled(1.0F));
}

void Direct2DRenderer::write_tab_title(std::string_view text, const core::LayoutRect &title,
                                       core::RgbColor color)
{
    // 題名の文字組みは帯の保持から引く（#924）。書式に焼いた文字単位の省略記号ごと保持し、
    // DrawTextW と同じ画素（tests/ui の GutterLayoutTests）。
    const auto shown = tab_layouts_.obtain(
        PaletteRowKey{tab_format_.Get(), PaletteTextRole::plain, text, title, dpi_},
        [&] { return text_layout(text, tab_format_.Get(), title); });
    if (!shown)
    {
        return;
    }
    brush_->SetColor(to_color(color));
    context_->PushAxisAlignedClip(to_rect(title), D2D1_ANTIALIAS_MODE_ALIASED);
    context_->DrawTextLayout(
        D2D1::Point2F(static_cast<float>(title.left), static_cast<float>(title.top)), shown.Get(),
        brush_.Get());
    context_->PopAxisAlignedClip();
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
                                                         const core::BodyLayout &body,
                                                         const core::EditorSettings &settings,
                                                         const application::LineWindow &window)
{
    auto made = body_text_.get(application::TextLayoutRequest{
        text,
        settings,
        core::TextViewGeometry{std::max(core::width_of(body.content), 1), body.line_height, dpi_},
        core::DisplayForm::text,
        {},
        window});
    if (!made)
    {
        text_failure_ = RenderFailure::directwrite;
        return nullptr;
    }
    return std::move(made).value();
}

BodyTextPainter<ID2D1RenderTarget> Direct2DRenderer::body_painter()
{
    BodyTextPainter<ID2D1RenderTarget> painter(*context_.Get(), brush_.Get(), body_text_,
                                               text_failure_);
    painter.set_rows(body_top_, body_bottom_);
    return painter;
}

void Direct2DRenderer::draw_body_text(IDWriteTextLayout *text, const core::LayoutRect &area,
                                      D2D1_DRAW_TEXT_OPTIONS options)
{
    body_painter().draw_body_text(text, area, options);
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

std::vector<DWRITE_HIT_TEST_METRICS> Direct2DRenderer::runs_of(IDWriteTextLayout *text,
                                                               const core::LayoutRect &area,
                                                               DWRITE_TEXT_RANGE range)
{
    return body_painter().runs_of(text, area, range);
}

void Direct2DRenderer::fill_runs(IDWriteTextLayout *text, const core::LayoutRect &area,
                                 DWRITE_TEXT_RANGE range)
{
    for (const auto &run : runs_of(text, area, range))
    {
        context_->FillRectangle(
            D2D1::RectF(run.left, run.top, run.left + run.width, run.top + run.height),
            brush_.Get());
    }
}

void Direct2DRenderer::underline_runs(IDWriteTextLayout *text, const core::LayoutRect &area,
                                      DWRITE_TEXT_RANGE range, std::int32_t thickness)
{
    for (const auto &run : runs_of(text, area, range))
    {
        const auto bottom =
            run.top + run.height - static_cast<float>(core::to_pixels(caret_inset_dips, dpi_));
        context_->FillRectangle(D2D1::RectF(run.left, bottom - static_cast<float>(thickness),
                                            run.left + run.width, bottom),
                                brush_.Get());
    }
}

void Direct2DRenderer::tint_runs(IDWriteTextLayout *text, const core::LayoutRect &area,
                                 DWRITE_TEXT_RANGE range, core::RgbColor color)
{
    brush_->SetColor(to_color(color));
    body_painter().tint_runs(text, area, range);
}

void Direct2DRenderer::outline_runs(IDWriteTextLayout *text, const core::LayoutRect &area,
                                    DWRITE_TEXT_RANGE range, float stroke)
{
    const float inset = stroke / 2.0F;
    for (const auto &run : runs_of(text, area, range))
    {
        context_->DrawRectangle(D2D1::RectF(run.left + inset, run.top + inset,
                                            run.left + run.width - inset,
                                            run.top + run.height - inset),
                                brush_.Get(), stroke);
    }
}

DWRITE_TEXT_RANGE Direct2DRenderer::range_of(const application::LineView &line,
                                             const core::SelectionSpan &span,
                                             LineUtf16Evaluation &positions)
{
    const core::SelectionSpan shown = displayed(line.display, span);
    const UINT32 from = positions.units_of(shown.begin);
    const UINT32 stop = positions.units_of(shown.end);
    return DWRITE_TEXT_RANGE{from, stop - from};
}

// 置き換えた文字（`is_replaced` の桁）の UTF-16 の範囲。IME の変換中の行では inserted に
// 変換中の文字列が差し込んであるので、その位置以降の範囲を長さぶん右へずらす（#152）。
// 本文だけの行は長さ 0 の inserted を渡す。差し込み位置は桁の境目なので範囲をまたがない。
std::vector<DWRITE_TEXT_RANGE> Direct2DRenderer::replaced_ranges(const core::DisplayLine &line,
                                                                 DWRITE_TEXT_RANGE inserted)
{
    std::vector<DWRITE_TEXT_RANGE> ranges;
    LineUtf16Evaluation positions(line.text);
    for (std::size_t column = 0; column + 1 < line.starts.size(); ++column)
    {
        if (!core::is_replaced(line, column))
        {
            continue;
        }
        const UINT32 from = positions.units_of(core::Column{line.starts.at(column) + 1});
        const UINT32 stop = positions.units_of(core::Column{line.starts.at(column + 1) + 1});
        const UINT32 shift = from >= inserted.startPosition ? inserted.length : 0U;
        ranges.push_back(DWRITE_TEXT_RANGE{from + shift, stop - from});
    }
    return ranges;
}

void Direct2DRenderer::draw_line_matches(const application::EditorFrame &frame,
                                         IDWriteTextLayout *text, const core::LayoutRect &area,
                                         const application::LineView &line)
{
    brush_->SetColor(to_color(frame.palette.search));
    LineUtf16Evaluation positions(line.display.text);
    for (const auto &span : line.matches)
    {
        fill_runs(text, area, range_of(line, span, positions));
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
    LineUtf16Evaluation positions(line.display.text);
    outline_runs(text, area, range_of(line, line.current_match.value(), positions), scaled(1.0F));
}

void Direct2DRenderer::draw_line_selection(const application::EditorFrame &frame,
                                           IDWriteTextLayout *text, const core::LayoutRect &area,
                                           const application::LineView &line)
{
    LineUtf16Evaluation positions(line.display.text);
    const DWRITE_TEXT_RANGE range = range_of(line, line.selection, positions);
    const UINT32 stop = range.startPosition + range.length;
    brush_->SetColor(to_color(frame.palette.selection));
    fill_runs(text, area, range);
    // 行末を越えたかは本文の桁で見る（描画の桁は末尾に畳まれている）。
    if (line.selection.end.value <= core::code_point_count(line.text) + 1)
    {
        return;
    }
    // 行をまたぐ選択は、改行のぶんだけ行末からはみ出して塗る（採用案 第 1 節）。
    const auto point = text_layout_point(text, stop, core::CaretAffinity::trailing);
    if (!point)
    {
        text_failure_ = RenderFailure::directwrite;
        return;
    }
    const float x = static_cast<float>(area.left) + point.value().x;
    const float top = static_cast<float>(area.top) + point.value().y;
    const auto mark = static_cast<float>(core::to_pixels(newline_mark_dips, dpi_));
    context_->FillRectangle(
        D2D1::RectF(x, top, x + mark, top + static_cast<float>(body_line_height_)), brush_.Get());
}

std::optional<core::LayoutRect> Direct2DRenderer::bar_of(IDWriteTextLayout *text,
                                                         const core::LayoutRect &area,
                                                         UINT32 position,
                                                         core::CaretAffinity affinity)
{
    const auto point = text_layout_point(text, position, affinity);
    if (!point)
    {
        text_failure_ = RenderFailure::directwrite;
        return std::nullopt;
    }
    const auto inset = core::to_pixels(caret_inset_dips, dpi_);
    const auto left = area.left + static_cast<std::int32_t>(point.value().x);
    const auto top = area.top + static_cast<std::int32_t>(point.value().y);
    return core::LayoutRect{left, top + inset, left + caret_width_,
                            top + body_line_height_ - inset};
}

void Direct2DRenderer::draw_bar_caret(const application::EditorFrame &frame,
                                      IDWriteTextLayout *text, const core::LayoutRect &area,
                                      UINT32 position)
{
    const auto affinity =
        frame.composition.has_value() ? core::CaretAffinity::leading : frame.caret.affinity;
    const auto bar = bar_of(text, area, position, affinity);
    if (!bar.has_value())
    {
        return;
    }
    const auto &shown = bar.value();
    fill(shown, frame.palette.accent);
    caret_rectangle_ = RECT{shown.left, shown.top, shown.right, shown.bottom};
}

std::optional<std::vector<DWRITE_HIT_TEST_METRICS>>
Direct2DRenderer::block_runs(IDWriteTextLayout *text, const core::LayoutRect &area,
                             const application::LineView &shown,
                             const application::CaretView &caret)
{
    const auto &line = shown.display;
    const auto column = windowed_column(caret, shown);
    LineUtf16Evaluation positions(line.text);
    const auto begin = positions.units_of(displayed(line, column));
    const auto end = positions.units_of(displayed(line, core::Column{column.value + 1}));
    const auto point = text_layout_point(text, begin, caret.affinity);
    if (!point)
    {
        text_failure_ = RenderFailure::directwrite;
        return std::nullopt;
    }
    auto runs = runs_of(text, area, DWRITE_TEXT_RANGE{begin, end - begin});
    if (runs.empty())
    {
        DWRITE_HIT_TEST_METRICS empty{};
        empty.left = static_cast<float>(area.left) + point.value().x;
        empty.top = static_cast<float>(area.top) + point.value().y;
        empty.width = static_cast<float>(core::to_pixels(block_minimum_dips, dpi_));
        empty.height = static_cast<float>(body_line_height_);
        runs.push_back(empty);
    }
    return runs;
}

void Direct2DRenderer::draw_block_caret(const application::EditorFrame &frame,
                                        IDWriteTextLayout *text, const core::LayoutRect &area,
                                        const application::LineView &shown)
{
    const auto made = block_runs(text, area, shown, frame.caret);
    if (!made.has_value())
    {
        return;
    }
    const auto &runs = made.value();
    const auto &first = runs.front();
    caret_rectangle_ = RECT{static_cast<LONG>(first.left), static_cast<LONG>(first.top),
                            static_cast<LONG>(first.left + first.width),
                            static_cast<LONG>(first.top + first.height)};
    const auto radius = static_cast<float>(core::to_pixels(block_radius_dips, dpi_));
    for (const auto &run : runs)
    {
        const auto block =
            D2D1::RectF(run.left, run.top, run.left + run.width, run.top + run.height);
        brush_->SetColor(to_color(frame.palette.accent));
        context_->FillRoundedRectangle(D2D1::RoundedRect(block, radius, radius), brush_.Get());
        context_->PushAxisAlignedClip(block, D2D1_ANTIALIAS_MODE_ALIASED);
        brush_->SetColor(to_color(frame.palette.on_accent));
        draw_body_text(text, area, D2D1_DRAW_TEXT_OPTIONS_NONE);
        context_->PopAxisAlignedClip();
    }
}

void Direct2DRenderer::draw_caret(const application::EditorFrame &frame, IDWriteTextLayout *text,
                                  const core::LayoutRect &area, const application::LineView &shown)
{
    const auto &line = shown.display;
    LineUtf16Evaluation positions(line.text);
    switch (frame.caret.shape)
    {
    case core::CaretShape::bar:
        draw_bar_caret(frame, text, area,
                       positions.units_of(displayed(line, windowed_column(frame.caret, shown))));
        return;
    case core::CaretShape::block:
        draw_block_caret(frame, text, area, shown);
        return;
    }
    std::unreachable();
}

// もう 1 つの枠の印（採用案の案 A）。色は呼び手が muted に置く。縦棒は操作中と同じ寸法、ブロックは
// 操作中と同じ範囲（block_runs）を 1 DIP の枠線で囲み、中は塗らない。IME の候補窓の位置
// （caret_rectangle_）は書かない（ADR 0148 の決定 10）。
void Direct2DRenderer::draw_passive_caret(IDWriteTextLayout *text, const core::LayoutRect &area,
                                          const application::LineView &shown,
                                          const application::CaretView &caret)
{
    switch (caret.shape)
    {
    case core::CaretShape::bar:
    {
        // 位置を引けなければ空の矩形（何も塗らない）。
        LineUtf16Evaluation positions(shown.display.text);
        const auto column = displayed(shown.display, windowed_column(caret, shown));
        const auto bar = bar_of(text, area, positions.units_of(column), caret.affinity);
        context_->FillRectangle(to_rect(bar.value_or(core::LayoutRect{})), brush_.Get());
        return;
    }
    case core::CaretShape::block:
        outline_block(block_runs(text, area, shown, caret));
        return;
    }
    std::unreachable();
}

// 区間の桁は本文の桁なので、選択と同じ range_of で描画用の行の UTF-16 へ写す（`^M` の展開も）。
// IME の差し込み行（#839）の区間は、差し込みの後ろが差し込んだ code point の数だけずれている。
// 本文の桁へ戻して差し込む前の行で写し、差し込んだ UTF-16 の数だけ後ろへずらす。差し込みを
// またぐ区間は application が割ってある。
std::vector<ColoredRun> Direct2DRenderer::colored_runs(const application::EditorFrame &frame,
                                                       const application::LineView &line)
{
    LineUtf16Evaluation positions(line.display.text);
    const auto at = line.composed.has_value()
                        ? application::composed_source_column(line.display, line.composed.value())
                        : core::Column{std::numeric_limits<std::size_t>::max()};
    const std::size_t count =
        line.composed.has_value() ? line.composed.value().inserted_columns : 0;
    const UINT32 inserted =
        line.composed.has_value()
            ? utf16_at(line.composed.value().text, line.composed.value().insertion.end.value) -
                  utf16_at(line.composed.value().text, line.composed.value().insertion.begin.value)
            : 0;
    std::vector<ColoredRun> colored;
    colored.reserve(line.colors.size());
    for (const auto &span : line.colors)
    {
        const bool after = span.begin.value >= at.value + count;
        const std::size_t back = after ? count : 0;
        auto range = range_of(line,
                              core::SelectionSpan{core::SelectionPresence::present,
                                                  core::Column{span.begin.value - back},
                                                  core::Column{span.end.value - back}},
                              positions);
        range.startPosition += after ? inserted : 0;
        if (range.length > 0)
        {
            colored.push_back(
                ColoredRun{range, to_color(core::syntax_color(frame.body, span.role))});
        }
    }
    return colored;
}

void Direct2DRenderer::draw_line_text(const application::EditorFrame &frame,
                                      IDWriteTextLayout *text, const core::LayoutRect &area,
                                      const application::LineView &line)
{
    if (line.colors.empty())
    {
        brush_->SetColor(to_color(frame.palette.text));
        draw_body_text(text, area);
        return;
    }
    const auto colored = colored_runs(frame, line);
    body_painter().draw_colored_body_text(
        text, area, BodyColors{colored, to_color(frame.palette.text)},
        [this](const D2D1_COLOR_F &color) { brush_->SetColor(color); });
}

void Direct2DRenderer::outline_block(
    const std::optional<std::vector<DWRITE_HIT_TEST_METRICS>> &runs)
{
    const float stroke = scaled(1.0F);
    const float inset = stroke / 2.0F;
    for (const auto &run : runs.value_or(std::vector<DWRITE_HIT_TEST_METRICS>{}))
    {
        context_->DrawRectangle(D2D1::RectF(run.left + inset, run.top + inset,
                                            run.left + run.width - inset,
                                            run.top + run.height - inset),
                                brush_.Get(), stroke);
    }
}

// 置き換えの描き直しは BodyTextPainter の 1 本（保持した字形 run から切り抜きと描く run
// を引く・#531）。
void Direct2DRenderer::draw_replaced(const application::EditorFrame &frame, IDWriteTextLayout *text,
                                     const core::LayoutRect &area,
                                     std::span<const DWRITE_TEXT_RANGE> ranges)
{
    if (ranges.empty())
    {
        return;
    }
    brush_->SetColor(to_color(frame.palette.muted));
    body_painter().draw_replaced(text, area, ranges);
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
    // 本文の変換と入力行の変換は同時に値を持たない（ADR 0061 の決定 4）ので、在る方を引く。
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
    if (!line.composed.has_value())
    {
        return;
    }
    const auto &shown = line.composed.value();
    LineUtf16Evaluation positions(shown.text);
    const UINT32 base = positions.units_of(shown.insertion_column);
    const auto text = layout_of(shown.text, body, frame.settings, line.window);
    if (!text)
    {
        return;
    }
    draw_line_text(frame, text.Get(), area, line);
    const UINT32 stop = utf16_at(shown.text, shown.insertion.end.value);
    draw_replaced(frame, text.Get(), area,
                  replaced_ranges(line.display, DWRITE_TEXT_RANGE{base, stop - base}));
    draw_clauses(frame, text.Get(), area, base);
    draw_bar_caret(frame, text.Get(), area, positions.units_of(shown.cursor));
}

void Direct2DRenderer::draw_plain_line(const application::EditorFrame &frame, const BodyPane &pane,
                                       const core::LayoutRect &area,
                                       const application::LineView &line)
{
    const auto text = layout_of(line.display.text, pane.body, frame.settings, line.window);
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
    draw_line_text(frame, text.Get(), area, line);
    draw_replaced(frame, text.Get(), area, replaced_ranges(line.display, DWRITE_TEXT_RANGE{0, 0}));
    draw_current_match(frame, text.Get(), area, line);
    if (line.number != pane.caret.position.line)
    {
        return;
    }
    switch (pane.role)
    {
    case PaneRole::active:
        if (!body_caret_hidden(frame))
        {
            draw_caret(frame, text.Get(), area, line);
        }
        return;
    case PaneRole::passive:
        brush_->SetColor(to_color(frame.palette.muted));
        draw_passive_caret(text.Get(), area, line, pane.caret);
        return;
    }
    std::unreachable();
}

// 現在行の面と行番号の強調は操作中の枠だけ（採用案の案 A）。もう 1 つの枠は同じ関数を通り、
// キャレットの印だけを受け身で描く（ADR 0148 の決定 8）。
void Direct2DRenderer::draw_line(const application::EditorFrame &frame, const BodyPane &pane,
                                 const application::LineView &line)
{
    const auto &body = pane.body;
    const auto top = row_pixel(body, line.screen_row);
    const auto bottom =
        row_pixel(body, line.screen_row + static_cast<std::int64_t>(line.screen_rows));
    const core::LayoutRect row{body.gutter.right, top, body.content.right, bottom};
    const bool current = pane.role == PaneRole::active && line.number == pane.caret.position.line;
    if (current)
    {
        fill(row, frame.palette.current_line);
    }
    const auto padding = gutter_padding(frame.settings);
    const auto label =
        core::gutter_label(line.number, pane.caret.position.line, frame.settings.relativenumber);
    write_gutter(
        label.text, label.alignment,
        gutter_number_area({body.gutter.left, top, body.gutter.right, top + body.line_height},
                           label.alignment, padding),
        current ? frame.palette.text : frame.palette.gutter);
    // 巨大な行の印は行番号の隣の 1 文字（ADR 0139 の決定 2）。行番号の右の余白に置く。
    if (line.window.active)
    {
        write_gutter(giant_mark, core::GutterAlignment::trailing,
                     core::LayoutRect{body.gutter.right - padding, top, body.gutter.right,
                                      top + body.line_height},
                     frame.palette.giant_mark);
    }
    // 原点のずれ（横の送りと巨大な行の窓の x）は application が行ごとに決める（ADR 0139 の決定
    // 5）。
    constexpr auto reach = static_cast<float>(std::numeric_limits<std::int32_t>::max() / 2);
    const auto left = static_cast<std::int32_t>(std::clamp(line.left, -reach, reach));
    const core::LayoutRect area{body.content.left + left, top, body.content.right, bottom};
    context_->PushAxisAlignedClip(to_rect(body.content), D2D1_ANTIALIAS_MODE_ALIASED);
    if (line.composed.has_value())
    {
        draw_composed_line(frame, body, area, line);
    }
    if (!line.composed.has_value())
    {
        draw_plain_line(frame, pane, area, line);
    }
    context_->PopAxisAlignedClip();
}

std::int32_t Direct2DRenderer::gutter_padding(const core::EditorSettings &settings) const
{
    return core::to_pixels(static_cast<std::int32_t>(static_cast<float>(gutter_padding_dips) *
                                                         core::font_size_ratio(settings.font_size) +
                                                     0.5F),
                           dpi_);
}

void Direct2DRenderer::draw_grid_rows(const application::EditorFrame &frame, const BodyPane &pane)
{
    if (pane.grid.empty())
    {
        return;
    }
    const auto &body = pane.body;
    const core::TextViewGeometry geometry{std::max(core::width_of(body.content), 1),
                                          body.line_height, dpi_};
    const auto cell = body_text_.cell(
        application::TextLayoutRequest{{}, frame.settings, geometry, core::DisplayForm::grid});
    if (!cell)
    {
        text_failure_ = RenderFailure::directwrite;
        return;
    }
    grid_cell_ = cell.value().width;
    GridPainter painter(*context_.Get(), brush_.Get(), body_text_, text_failure_);
    for (const auto &row : pane.grid)
    {
        draw_grid_row(painter, frame, pane, row);
    }
}

// 行番号は行の最初の画面行だけ。キャレットの行は画面行ごとに current_line で塗る（操作中の枠
// だけ）。もう 1 つの枠は変換を差し込まず、入力行が開いていても受け身の印を描く。
void Direct2DRenderer::draw_grid_row(GridPainter &painter, const application::EditorFrame &frame,
                                     const BodyPane &pane, const application::GridRowView &row)
{
    const auto &body = pane.body;
    const auto top = row_pixel(body, row.screen_row);
    const auto bottom = top + body.line_height;
    const bool active = pane.role == PaneRole::active;
    const bool current = active && row.number == pane.caret.position.line;
    const std::optional<application::CompositionView> no_composition;
    if (current)
    {
        fill(core::LayoutRect{body.gutter.right, top, body.content.right, bottom},
             frame.palette.current_line);
    }
    if (row.row == 0)
    {
        const auto label =
            core::gutter_label(row.number, pane.caret.position.line, frame.settings.relativenumber);
        write_gutter(label.text, label.alignment,
                     gutter_number_area({body.gutter.left, top, body.gutter.right, bottom},
                                        label.alignment, gutter_padding(frame.settings)),
                     current ? frame.palette.text : frame.palette.gutter);
    }
    const GridRowCanvas canvas{
        core::LayoutRect{body.content.left, top, body.content.right, bottom},
        grid_cell_,
        frame.settings,
        core::TextViewGeometry{std::max(core::width_of(body.content), 1), body.line_height, dpi_},
        frame.palette,
        frame.body,
        pane.caret.shape,
        !active || !body_caret_hidden(frame),
        active ? frame.composition : no_composition,
        caret_width_,
        core::to_pixels(caret_inset_dips, dpi_),
        core::to_pixels(newline_mark_dips, dpi_),
        static_cast<float>(core::to_pixels(block_radius_dips, dpi_)),
        core::to_pixels(target_underline_dips, dpi_),
        core::to_pixels(other_underline_dips, dpi_),
        scaled(1.0F),
        pane.role};
    context_->PushAxisAlignedClip(to_rect(body.content), D2D1_ANTIALIAS_MODE_ALIASED);
    const auto caret = painter.draw(row, canvas);
    context_->PopAxisAlignedClip();
    if (caret.has_value() && active)
    {
        caret_rectangle_ = caret.value();
    }
}

void Direct2DRenderer::draw_body(const application::EditorFrame &frame,
                                 const core::BodyLayout &body)
{
    draw_pane(frame, BodyPane{body, frame.lines, frame.grid, frame.caret, PaneRole::active});
}

// もう 1 つの枠と境界（ADR 0148 の決定 8・採用案の案 A）。境界は panel_border の 1 本。frame に
// もう 1 つの枠が載らない間（見つからない文書）も、その側の帯は背景で塗る。
void Direct2DRenderer::draw_other_body(const application::EditorFrame &frame,
                                       const core::PaneLayouts &panes)
{
    if (panes.divider.has_value())
    {
        fill(panes.divider.value(), frame.palette.panel_border);
    }
    const auto layout = core::other_pane_layout(panes, frame.active_side);
    if (!layout.has_value())
    {
        return;
    }
    if (!frame.other.has_value())
    {
        draw_pane(frame, BodyPane{layout.value(), {}, {}, frame.caret, PaneRole::passive});
        return;
    }
    std::visit([this, &frame, &layout](const auto &other)
               { this->draw_other(frame, layout.value(), other); }, frame.other.value());
}

void Direct2DRenderer::draw_other(const application::EditorFrame &frame,
                                  const core::BodyLayout &body, const application::PaneFrame &other)
{
    const application::CaretView caret{other.caret.position, passive_shape(frame.mode),
                                       other.caret.affinity};
    draw_pane(frame, BodyPane{body, other.lines, other.grid, caret, PaneRole::passive});
}

// プレビューの枠（ADR 0155 の決定 7・11）。地は受け身の本文の枠と同じく枠の帯の全体を塗り、
// 知らせがあれば枠の上の 1 行に本文の色で左から書いて、要素はその下から描く。
void Direct2DRenderer::draw_other(const application::EditorFrame &frame,
                                  const core::BodyLayout &body,
                                  const application::PreviewFrame &other)
{
    fill(body.band, frame.palette.background);
    const PreviewTextStyle style{std::string(frame.settings.font_family.text()),
                                 frame.settings.font_size.points(), dpi_};
    auto area = body.band;
    if (other.notice.has_value())
    {
        const auto row = static_cast<std::int32_t>(preview_line_pixels(style));
        const core::LayoutRect notice{area.left +
                                          static_cast<std::int32_t>(preview_side_margin(dpi_)),
                                      area.top, area.right, std::min(area.top + row, area.bottom)};
        context_->PushAxisAlignedClip(to_rect(notice), D2D1_ANTIALIAS_MODE_ALIASED);
        write(other.notice.value().text(), command_format_.Get(), notice, frame.palette.text);
        context_->PopAxisAlignedClip();
        area.top = notice.bottom;
    }
    const auto prepared =
        preview_.prepare(*context_.Get(), core::selected_theme(frame.settings, frame.appearance));
    const auto painted = prepared ? preview_.paint(*context_.Get(), other, style, area) : prepared;
    if (!painted && !text_failure_.has_value())
    {
        text_failure_ = painted.error();
    }
}

// 1 つの枠の本文。body_top_ / body_bottom_ は描いている枠の本文の上下（字形 run の間引き）で、
// 描き終えたら戻す。
void Direct2DRenderer::draw_pane(const application::EditorFrame &frame, const BodyPane &pane)
{
    const auto &body = pane.body;
    fill(body.band, frame.palette.background);
    // 行番号の列の地は帯の上端から下端まで塗る（本文の上の余白も gutter の色・#794）。
    fill(core::LayoutRect{body.gutter.left, body.band.top, body.gutter.right, body.band.bottom},
         frame.palette.gutter_background);
    caret_width_ = body.caret_width;
    body_line_height_ = body.line_height;
    // 問いの答えを待つ間は本文を描かない（ADR 0140 の決定 2・判定はここの 1 か所）。
    if (frame.display_pending)
    {
        return;
    }
    const core::LayoutRect visible{body.band.left, body.content.top, body.band.right,
                                   body.band.bottom};
    body_top_ = body.content.top;
    body_bottom_ = body.content.bottom;
    context_->PushAxisAlignedClip(to_rect(visible), D2D1_ANTIALIAS_MODE_ALIASED);
    for (const auto &line : pane.lines)
    {
        draw_line(frame, pane, line);
    }
    draw_grid_rows(frame, pane);
    context_->PopAxisAlignedClip();
    body_top_ = std::numeric_limits<std::int32_t>::min();
    body_bottom_ = std::numeric_limits<std::int32_t>::max();
}

std::int32_t Direct2DRenderer::body_guide_key_width(const application::EditorFrame &frame)
{
    // 本文の案内が出うる文脈だけで測る（ふだんの frame は layout を作らない・#1122）。
    if (frame.guide.context != core::GuideContext::untouched_untitled)
    {
        return 0;
    }
    const std::array<std::string, 3> keys{frame.guide.entries.at(0).key,
                                          frame.guide.entries.at(1).key,
                                          frame.guide.entries.at(2).key};
    if (measured_guide_keys_.has_value() && measured_guide_keys_.value() == keys)
    {
        return measured_guide_key_width_;
    }
    measured_guide_key_width_ =
        guide_key_width(*dwrite_.Get(), *guide_key_format_.Get(), frame.guide.entries,
                        scaled(guide_key_padding_dips));
    measured_guide_keys_ = keys;
    return measured_guide_key_width_;
}

void Direct2DRenderer::draw_body_guide(const application::EditorFrame &frame,
                                       const core::OperationGuideLayout &guide)
{
    const auto *body = std::get_if<core::BodyGuideLayout>(&guide);
    if (body == nullptr)
    {
        return;
    }
    const KeycapStyle style{
        guide_key_padding_dips, 20.0F, 5.0F, frame.palette.panel, frame.palette.panel_border,
        frame.palette.text};
    for (std::size_t index = 0; index < body->rows.size(); ++index)
    {
        const auto &row = body->rows.at(index);
        const auto &entry = frame.guide.entries.at(index);
        context_->PushAxisAlignedClip(to_rect(row.key), D2D1_ANTIALIAS_MODE_ALIASED);
        draw_keycap(entry.key, guide_key_format_.Get(), row.key, style);
        context_->PopAxisAlignedClip();
        context_->PushAxisAlignedClip(to_rect(row.label), D2D1_ANTIALIAS_MODE_ALIASED);
        write(entry.body_name, guide_label_format_.Get(), row.label, frame.palette.muted);
        context_->PopAxisAlignedClip();
    }
    context_->PushAxisAlignedClip(to_rect(body->note), D2D1_ANTIALIAS_MODE_ALIASED);
    write("打ち始めると消えます", guide_note_format_.Get(), body->note, frame.palette.muted);
    context_->PopAxisAlignedClip();
}

void Direct2DRenderer::draw_status_guide(const application::EditorFrame &frame,
                                         const core::OperationGuideLayout &guide)
{
    const auto *status = std::get_if<core::StatusGuideLayout>(&guide);
    if (status == nullptr)
    {
        return;
    }
    constexpr std::array<std::size_t, 2> entries{0, 2};
    for (std::size_t index = 0; index < status->rows.size(); ++index)
    {
        const auto &row = status->rows.at(index);
        const auto &entry = frame.guide.entries.at(entries.at(index));
        context_->PushAxisAlignedClip(to_rect(row.key), D2D1_ANTIALIAS_MODE_ALIASED);
        write_status(entry.key, status_format_.Get(), row.key, frame.palette.text);
        context_->PopAxisAlignedClip();
        context_->PushAxisAlignedClip(to_rect(row.label), D2D1_ANTIALIAS_MODE_ALIASED);
        write_status(entry.short_name, guide_status_format_.Get(), row.label, frame.palette.muted);
        context_->PopAxisAlignedClip();
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
        // 狭い窓で畳んだ言語の欄（幅 0・ADR 0154 の決定 6）は書かず、枠も使わない。
        if (core::width_of(layout.items.at(index)) <= 0)
        {
            continue;
        }
        write_status(frame.status_items.at(index).text(), status_format_.Get(),
                     layout.items.at(index), frame.palette.muted);
    }
    // 表示形式の印（ADR 0140 の決定 3）。文字は core の表、右寄せは write_right の 1 本。
    const auto mark = core::display_form_label(frame.display);
    if (!mark.empty())
    {
        write_right(mark, status_format_.Get(), layout.display, frame.palette.muted);
    }
}

void Direct2DRenderer::draw_status_left(const application::EditorFrame &frame,
                                        const core::StatusBarLayout &layout)
{
    const auto command = core::command_layout(layout, dpi_, 0);
    // 分かれ目は status_left_state の 1 か所で、枠の数は status_layout_demand が同じ値から数える。
    switch (status_left_state(frame))
    {
    case StatusLeftState::command_input:
        draw_command(frame, core::command_input_label(command, dpi_));
        draw_completions(frame, layout);
        return;
    case StatusLeftState::message:
        draw_status_message(frame, core::command_input_label(command, dpi_));
        return;
    case StatusLeftState::mode:
    case StatusLeftState::recording:
        break;
    }
    draw_toggle(frame, layout);
    const auto &mode = status_text_layout(frame.mode_label, mode_format_.Get(), layout.mode);
    draw_status_text(mode, layout.mode, frame.palette.text);
    draw_recording(frame, layout, mode);
}

void Direct2DRenderer::draw_status_message(const application::EditorFrame &frame,
                                           const core::LayoutRect &area)
{
    if (!frame.command_message.has_value())
    {
        return;
    }
    context_->PushAxisAlignedClip(to_rect(area), D2D1_ANTIALIAS_MODE_ALIASED);
    write_status(frame.command_message.value().text(), command_format_.Get(), area,
                 frame.palette.text);
    context_->PopAxisAlignedClip();
}

// 録画中のマクロ（ADR 0046 の決定 8）。モード表示の文字の直後から右の項目の手前までに、
// Vim のコマンド行と同じ `recording @a` を muted で書く。色はトークンだけ（ADR 0008 決定 8）。
void Direct2DRenderer::draw_recording(const application::EditorFrame &frame,
                                      const core::StatusBarLayout &layout,
                                      const StatusTextLayout &mode)
{
    if (!frame.recording.has_value())
    {
        return;
    }
    DWRITE_TEXT_METRICS metrics{};
    if (!mode.layout || FAILED(mode.layout->GetMetrics(&metrics)))
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
    // 窓の先頭は frame の値（ADR 0150 の決定 1）。知らせの 1 行に替えたときは候補の数で切り詰める。
    const auto start = std::min(command.completion_first, candidates.size() - layout.visible_rows);
    context_->PushAxisAlignedClip(to_rect(layout.panel), D2D1_ANTIALIAS_MODE_ALIASED);
    for (std::size_t index = 0; index < layout.visible_rows; ++index)
    {
        const auto top = layout.panel.top + static_cast<std::int32_t>(index) * layout.row_height;
        const core::LayoutRect row{layout.panel.left, top, layout.panel.right,
                                   top + layout.row_height};
        draw_completion(frame, row, candidates.at(start + index), selected == start + index);
    }
    context_->PopAxisAlignedClip();
    brush_->SetColor(to_color(frame.palette.panel_border));
    context_->DrawRectangle(to_rect(layout.panel), brush_.Get(), scaled(1.0F));
}

// 補完の 1 行。選択の面は行いっぱい、文字は左右 12 DIP 内側（#786）。長い候補は文字の矩形の右で
// 切れる。
void Direct2DRenderer::draw_completion(const application::EditorFrame &frame,
                                       const core::LayoutRect &row, std::string_view candidate,
                                       bool active)
{
    if (active)
    {
        fill(row, frame.palette.accent);
    }
    const auto label = core::command_row_label(row, dpi_);
    if (core::width_of(label) <= 0)
    {
        return;
    }
    context_->PushAxisAlignedClip(to_rect(label), D2D1_ANTIALIAS_MODE_ALIASED);
    write(candidate, command_format_.Get(), label,
          active ? frame.palette.on_accent : frame.palette.text);
    context_->PopAxisAlignedClip();
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
    draw_palette_label(frame, choice, label);
    context_->PopAxisAlignedClip();
    if (choice.origin.has_value())
    {
        write_palette_note(core::palette_origin_label(choice.origin.value()),
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
    // draw_keycap と同じく、寸法を確かめてから layout を引く（0 以下の欄で作らない・#900）。
    if (core::width_of(note) <= 0 || core::height_of(note) <= 0)
    {
        return;
    }
    const auto shown = palette_text(key, key_format_.Get(), PaletteTextRole::plain, note);
    draw_keycap_layout(shown.Get(), note,
                       KeycapStyle{palette_key_padding_dips, palette_key_height_dips,
                                   palette_key_radius_dips, frame.palette.background,
                                   frame.palette.panel_border, frame.palette.text});
}

void Direct2DRenderer::write_palette_note(std::string_view text, const core::LayoutRect &area,
                                          core::RgbColor color)
{
    // write_right と同じ描き方で、文字組みだけを面の保持から引く（#900）。
    if (core::width_of(area) <= 0 || core::height_of(area) <= 0)
    {
        return;
    }
    const auto shown = palette_text(text, status_format_.Get(), PaletteTextRole::trailing, area);
    if (!shown)
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

Direct2DRenderer::TextLayout Direct2DRenderer::palette_text(std::string_view text,
                                                            IDWriteTextFormat *format,
                                                            PaletteTextRole role,
                                                            const core::LayoutRect &area)
{
    return palette_layouts_.obtain(PaletteRowKey{format, role, text, area, dpi_},
                                   [&] { return made_palette_text(text, format, role, area); });
}

Direct2DRenderer::TextLayout Direct2DRenderer::made_palette_text(std::string_view text,
                                                                 IDWriteTextFormat *format,
                                                                 PaletteTextRole role,
                                                                 const core::LayoutRect &area)
{
    auto made = text_layout(text, format, area);
    if (!made)
    {
        return made;
    }
    switch (role)
    {
    case PaletteTextRole::plain:
        return made;
    case PaletteTextRole::trimmed:
        return SUCCEEDED(trim_by_character(made.Get())) ? made : nullptr;
    case PaletteTextRole::trailing:
        return SUCCEEDED(made->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING)) ? made : nullptr;
    }
    std::unreachable();
}

void Direct2DRenderer::draw_keycap(std::string_view key, IDWriteTextFormat *format,
                                   const core::LayoutRect &note, const KeycapStyle &style)
{
    // 文字の左右 9 DIP・高さ 20 DIP・角丸 5 DIP の枠を、右端を欄の右端に揃えて行の縦の中央に置く。
    // 幅は左寄せの書式の layout で測る（右寄せの書式で測らない・#258）。
    // 欄より広ければ欄の幅で切る。
    if (core::width_of(note) <= 0 || core::height_of(note) <= 0)
    {
        return;
    }
    const auto shown = text_layout(key, format, note);
    draw_keycap_layout(shown.Get(), note, style);
}

void Direct2DRenderer::draw_keycap_layout(IDWriteTextLayout *shown, const core::LayoutRect &note,
                                          const KeycapStyle &style)
{
    DWRITE_TEXT_METRICS metrics{};
    if (core::width_of(note) <= 0 || core::height_of(note) <= 0 || shown == nullptr ||
        FAILED(shown->GetMetrics(&metrics)))
    {
        return;
    }
    const float padding = scaled(style.padding);
    const float stroke = scaled(1.0F);
    const auto right = static_cast<float>(note.right);
    const float width =
        std::min(metrics.width + (padding * 2.0F), static_cast<float>(core::width_of(note)));
    const float middle = static_cast<float>(note.top + note.bottom) / 2.0F;
    const float half_height = scaled(style.height) / 2.0F;
    const D2D1_RECT_F box{right - width + (stroke / 2.0F), middle - half_height,
                          right - (stroke / 2.0F), middle + half_height};
    const float radius = scaled(style.radius);
    brush_->SetColor(to_color(style.face));
    context_->FillRoundedRectangle(D2D1::RoundedRect(box, radius, radius), brush_.Get());
    brush_->SetColor(to_color(style.border));
    context_->DrawRoundedRectangle(D2D1::RoundedRect(box, radius, radius), brush_.Get(), stroke);
    brush_->SetColor(to_color(style.text));
    context_->PushAxisAlignedClip(box, D2D1_ANTIALIAS_MODE_ALIASED);
    context_->DrawTextLayout(D2D1::Point2F(right - width + padding, static_cast<float>(note.top)),
                             shown, brush_.Get());
    context_->PopAxisAlignedClip();
}

void Direct2DRenderer::draw_palette_label(const application::EditorFrame &frame,
                                          const core::CommandChoice &choice,
                                          const core::LayoutRect &label)
{
    // 名前は場所の有無によらず同じ文字組みで、面の保持から引く（#900）。DrawTextW と同じ画素
    // （tests/ui の GutterLayoutTests）。
    const auto title =
        palette_text(choice.label.text(), command_format_.Get(), PaletteTextRole::plain, label);
    if (!title)
    {
        return;
    }
    brush_->SetColor(to_color(frame.palette.text));
    context_->DrawTextLayout(
        D2D1::Point2F(static_cast<float>(label.left), static_cast<float>(label.top)), title.Get(),
        brush_.Get());
    draw_palette_detail(frame, choice, label, title.Get());
}

void Direct2DRenderer::draw_palette_detail(const application::EditorFrame &frame,
                                           const core::CommandChoice &choice,
                                           const core::LayoutRect &label, IDWriteTextLayout *title)
{
    // 場所は題名の後ろに 12 DIP 空けて muted で書き、行の右端で文字単位に切る（ADR 0057 の
    // 決定 7）。題名が入りきらない行には書かない。detail の無い行（Ex の候補）は何もしない。
    if (!choice.detail.has_value())
    {
        return;
    }
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
    const auto detail = palette_text(choice.detail.value().text(), command_format_.Get(),
                                     PaletteTextRole::trimmed, place);
    if (!detail)
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
        // 行の名前と同じ欄に書く（ADR 0060 の決定 9）。面の左端に寄せない。#309
        write("候補なし", mode_format_.Get(),
              core::palette_row_label(core::palette_row(layout, 0), dpi_, false),
              frame.palette.muted);
        return;
    }
    const auto start = palette.first;
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
    text_failure_.reset();
    gutter_layouts_.begin();
    context_->SetTarget(surface);
    context_->BeginDraw();
    context_->Clear(unpainted());
    const auto size = context_->GetSize();
    const auto width = static_cast<std::int32_t>(size.width);
    const auto height = static_cast<std::int32_t>(size.height);
    const auto title = core::title_bar_layout(application::title_bar_input(frame, width, dpi_));
    const auto status = core::status_bar_layout(width, height, dpi_);
    // 面が出ていれば本文の枠は面の幅だけ右へ縮めた client から求める（ADR 0162 の決定 2・
    // ADR 0148 の 1 本はそのまま）。面が消えていれば body は client のまま。
    const auto explorer =
        core::explorer_layout(core::LayoutRect{0, 0, width, height}, dpi_, frame.settings.font_size,
                              frame.explorer.has_value());
    const auto panes =
        core::pane_layouts(explorer.body, dpi_, frame.settings.font_size, frame.layout);
    const auto body = core::active_pane_layout(panes, frame.active_side);
    const auto guide = core::operation_guide_layout(
        core::GuideRequest{frame.guide.context, body_guide_key_width(frame)}, body, status, dpi_);
    draw_title_bar(frame, title);
    draw_body(frame, body);
    draw_other_body(frame, panes);
    draw_explorer(frame, explorer);
    draw_body_guide(frame, guide);
    draw_status_bar(frame, status);
    draw_status_guide(frame, guide);
    if (frame.command_palette.has_value())
    {
        draw_palette(
            frame, core::palette_layout(width, height, dpi_, frame.command_palette.value().total));
    }
    else
    {
        // 面を閉じたら行の文字組みを捨てる。開くたびに作り直す（#900）。
        palette_layouts_.clear();
    }
    gutter_layouts_.end();
    const auto ended = context_->EndDraw();
    context_->SetTarget(nullptr);
    if (text_failure_.has_value())
    {
        return std::unexpected(text_failure_.value());
    }
    if (FAILED(ended))
    {
        return std::unexpected(classify(ended));
    }
    return {};
}

void Direct2DRenderer::draw_explorer(const application::EditorFrame &frame,
                                     const core::ExplorerLayout &layout)
{
    if (!frame.explorer.has_value())
    {
        // 面を消したら行の文字組みを捨てる。出すたびに作り直す（Ctrl+P の面と同じ・#900）。
        explorer_layouts_.clear();
        return;
    }
    ExplorerPainter painter(*context_.Get(), *brush_.Get(),
                            ExplorerText{dwrite_.Get(), explorer_format_.Get(), dpi_},
                            explorer_layouts_);
    painter.paint(frame.explorer.value(), layout, frame.palette);
}

std::expected<void, RenderFailure> Direct2DRenderer::acquire_surface()
{
    if (surface_ != nullptr)
    {
        return {};
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
    surface_ = std::move(bitmap);
    return {};
}

std::expected<void, RenderFailure> Direct2DRenderer::render(const application::EditorFrame &frame,
                                                            application::TimingPort &timing)
{
    const auto formatted = set_font(frame.settings);
    if (!formatted)
    {
        return formatted;
    }
    // flip model の待機可能オブジェクトで、描く前に 1 frame ぶんだけ待つ（ADR 0007 の追補・#888）。
    if (WaitForSingleObjectEx(latency_.get(), latency_timeout_milliseconds, TRUE) != WAIT_OBJECT_0)
    {
        return std::unexpected(RenderFailure::swap_chain);
    }
    const auto acquired = acquire_surface();
    if (!acquired)
    {
        return acquired;
    }
    const auto drawn = draw(frame, surface_.Get());
    if (!drawn)
    {
        return drawn;
    }
    // draw は EndDraw で終わる。ここが EndDraw の直後（#844）。
    timing.mark(core::Milestone::frame_drawn);
    // 待ちは描く前に済んでいる。垂直同期を待たずに合成へ渡す（ADR 0007 の追補・#888）。
    const auto presented = swap_chain_->Present(0, 0);
    if (FAILED(presented))
    {
        return std::unexpected(classify(presented));
    }
    return {};
}

std::expected<void, RenderFailure> Direct2DRenderer::resize(UINT width, UINT height)
{
    context_->SetTarget(nullptr);
    surface_.Reset();
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

bool Direct2DRenderer::provisional() const noexcept
{
    return provisional_;
}

HRESULT Direct2DRenderer::wait_for_commit() const
{
    return composition_->WaitForCommitCompletion();
}

std::optional<std::size_t> Direct2DRenderer::preview_task_at(std::int32_t x, std::int32_t y) const
{
    return preview_.task_at(x, y);
}
} // namespace nenenib::ui::win32
