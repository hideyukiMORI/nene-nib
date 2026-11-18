// 設定の面（右の引き出し・施主決定 D102・#1836）を offscreen（WIC の bitmap）に描いて PNG に落とす
// 確かめの道具（nib_preview_capture と同じ土台）。製品の旗は足さず、窓・device・swap chain を
// 作らない。描く経路は製品と同じ SettingsPanelFormats・SettingsPanelPainter・core::settings_layout
// で、行は core::settings_rows と既定の設定から作る（application の面の表示値と同じ作り）。帯は窓の
// client の全部（タイトルバーとステータスバーは描かない）で、面の外は配色の背景で塗る。
// CTest に登録しない（設計席が PNG を Read で見る）。
//
//   nib_settings_capture <png> <幅 px> <高さ px> <dpi> <配色の名前> <焦点の行|-1> <送り px> <lsp 0|1>
#include "DevicePixels.hpp"
#include "DisplayLanguage.hpp"
#include "EditorFeature.hpp"
#include "EditorSettings.hpp"
#include "FeatureCosts.hpp"
#include "LayoutRect.hpp"
#include "PaletteRowLayouts.hpp"
#include "SettingsPanel.hpp"
#include "SettingsPanelFormats.hpp"
#include "SettingsPanelPainter.hpp"
#include "SettingsRowView.hpp"
#include "SettingsRows.hpp"
#include "SettingsSectionTab.hpp"
#include "SettingsSubject.hpp"
#include "SettingsValue.hpp"
#include "SettingsView.hpp"
#include "Theme.hpp"
#include "ThemeCatalog.hpp"
#include "ThemeName.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <d2d1.h>
#include <dwrite.h>
#include <objbase.h>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <wincodec.h>
#include <wrl/client.h>

namespace
{
namespace core = nenenib::core;
namespace win32 = nenenib::ui::win32;
namespace app = nenenib::application;
using Bitmap = Microsoft::WRL::ComPtr<IWICBitmap>;
using Imaging = Microsoft::WRL::ComPtr<IWICImagingFactory>;
using Target = Microsoft::WRL::ComPtr<ID2D1RenderTarget>;

constexpr std::size_t argument_count = 9;
constexpr float full_channel = 255.0F;

[[nodiscard]] std::optional<std::int32_t> number_of(std::string_view text)
{
    std::int32_t value = 0;
    const auto *end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    return error == std::errc{} && stop == end ? std::optional{value} : std::nullopt;
}

// 表の 1 行の表示値（application の row_view と同じ作り）。
[[nodiscard]] app::SettingsRowView row_view(const core::SettingsRow &row,
                                            const core::EditorSettings &settings)
{
    app::SettingsRowView view{row,
                              core::settings_row_title(row.subject),
                              core::settings_row_description(row.subject),
                              core::settings_row_set_name(row.subject),
                              false,
                              false,
                              {},
                              std::nullopt,
                              false};
    if (std::holds_alternative<core::EditorFeature>(row.subject))
    {
        const auto feature = std::get<core::EditorFeature>(row.subject);
        view.on = core::feature_enabled(settings, feature);
        view.switchable = core::feature_switchable(feature);
        view.tier = core::feature_tier(feature);
        view.background = view.tier.has_value() && core::feature_cost(feature).background;
        return view;
    }
    view.value = core::settings_value_text(settings, std::get<core::SettingsValue>(row.subject));
    return view;
}

[[nodiscard]] app::SettingsView view_of(const core::EditorSettings &settings, std::int32_t focused,
                                        std::int32_t scroll)
{
    app::SettingsView view{};
    for (std::size_t index = 0; index < core::settings_sections.size(); ++index)
    {
        const auto section = core::settings_sections.at(index);
        view.tabs.at(index) =
            app::SettingsSectionTab{section, core::settings_section_title(section), true};
    }
    for (const auto &row : core::settings_rows)
    {
        view.rows.push_back(row_view(row, settings));
    }
    if (focused >= 0 && static_cast<std::size_t>(focused) < view.rows.size())
    {
        view.focused = static_cast<std::size_t>(focused);
        view.section = view.rows.at(view.focused.value_or(0)).row.section;
    }
    view.summary = core::speed_summary(settings);
    view.lsp_commands = settings.lsp_commands;
    view.scroll = scroll;
    return view;
}

// WIC の bitmap と、それに描く Direct2D のソフトウェアの描画先（DPI は core::reference_dpi）。
[[nodiscard]] HRESULT make_target(IWICImagingFactory &imaging, const core::LayoutRect &area,
                                  Bitmap &bitmap, Target &target)
{
    Microsoft::WRL::ComPtr<ID2D1Factory> factory;
    const auto reference = static_cast<float>(core::reference_dpi);
    const auto properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), reference,
        reference);
    auto result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf());
    if (SUCCEEDED(result))
    {
        result = imaging.CreateBitmap(static_cast<UINT>(area.right), static_cast<UINT>(area.bottom),
                                      GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bitmap);
    }
    if (SUCCEEDED(result))
    {
        result = factory->CreateWicBitmapRenderTarget(bitmap.Get(), properties, &target);
    }
    if (SUCCEEDED(result))
    {
        target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    }
    return result;
}

[[nodiscard]] HRESULT save_png(IWICImagingFactory &imaging, IWICBitmap &bitmap,
                               const std::string &path)
{
    const auto wide = core::to_utf16(path);
    Microsoft::WRL::ComPtr<IWICStream> stream;
    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    if (!wide || FAILED(imaging.CreateStream(&stream)) ||
        FAILED(stream->InitializeFromFilename(wide.value().c_str(), GENERIC_WRITE)) ||
        FAILED(imaging.CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) ||
        FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) ||
        FAILED(encoder->CreateNewFrame(&frame, nullptr)) || FAILED(frame->Initialize(nullptr)) ||
        FAILED(frame->WriteSource(&bitmap, nullptr)) || FAILED(frame->Commit()))
    {
        return E_FAIL;
    }
    return encoder->Commit();
}

[[nodiscard]] std::optional<core::Theme> theme_of(std::string_view name)
{
    static const auto catalog = core::ThemeCatalog::builtins();
    const auto parsed = core::ThemeName::parse(name);
    if (!parsed)
    {
        return std::nullopt;
    }
    const auto found = catalog.find(parsed.value());
    return found ? std::optional<core::Theme>{found.value().view()} : std::nullopt;
}

// 面を描く。失敗は false。
[[nodiscard]] bool paint(ID2D1RenderTarget &target, const app::SettingsView &view,
                         const core::Theme &theme, std::uint32_t dpi)
{
    Microsoft::WRL::ComPtr<IDWriteFactory> dwrite;
    Microsoft::WRL::ComPtr<IUnknown> unknown;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    win32::SettingsPanelFormats formats;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   unknown.GetAddressOf())) ||
        FAILED(unknown.As(&dwrite)) ||
        FAILED(target.CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brush)) ||
        !formats.made(*dwrite.Get(), {L"Segoe UI", L"Consolas"},
                      static_cast<float>(dpi) / static_cast<float>(core::reference_dpi), L"ja-jp"))
    {
        return false;
    }
    const auto size = target.GetSize();
    const core::LayoutRect area{0, 0, static_cast<std::int32_t>(size.width),
                                static_cast<std::int32_t>(size.height)};
    win32::PaletteRowLayouts layouts{win32::SettingsPanelPainter::retained};
    win32::SettingsPanelPainter painter(
        target, *brush.Get(), formats.text(dwrite.Get(), dpi, core::DisplayLanguage::japanese),
        layouts);
    const auto layout = painter.arranged(view, area);
    const auto &ground = theme.ui.background;
    target.BeginDraw();
    target.Clear(D2D1::ColorF(static_cast<float>(ground.red) / full_channel,
                              static_cast<float>(ground.green) / full_channel,
                              static_cast<float>(ground.blue) / full_channel, 1.0F));
    painter.paint(view, layout, theme.ui, theme.body.error);
    std::printf("settings: panel %d..%d, %zu items, scroll %d of %d, %zu layouts held\n",
                layout.panel.left, layout.panel.right, layout.items.size(), layout.scroll,
                layout.scroll_limit, layouts.size());
    return SUCCEEDED(target.EndDraw());
}

[[nodiscard]] int capture(std::span<char *> arguments)
{
    const auto width = number_of(arguments[2]);
    const auto height = number_of(arguments[3]);
    const auto dpi = number_of(arguments[4]);
    const auto theme = theme_of(arguments[5]);
    const auto focused = number_of(arguments[6]);
    const auto scroll = number_of(arguments[7]);
    const auto lsp = number_of(arguments[8]);
    if (!width || !height || !dpi || !theme || !focused || !scroll || !lsp)
    {
        return 2;
    }
    auto settings = core::default_editor_settings();
    settings.lsp = lsp.value() != 0;
    const auto view = view_of(settings, focused.value(), scroll.value());
    Imaging imaging;
    Bitmap bitmap;
    Target target;
    const core::LayoutRect area{0, 0, width.value(), height.value()};
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&imaging))) ||
        FAILED(make_target(*imaging.Get(), area, bitmap, target)) ||
        !paint(*target.Get(), view, theme.value(), static_cast<std::uint32_t>(dpi.value())) ||
        FAILED(save_png(*imaging.Get(), *bitmap.Get(), arguments[1])))
    {
        std::fputs("settings: the target, the painting or the file failed\n", stderr);
        return 1;
    }
    std::printf("settings: wrote %s (%d x %d px)\n", arguments[1], width.value(), height.value());
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    const std::span arguments(argv, static_cast<std::size_t>(argc));
    if (arguments.size() != argument_count)
    {
        std::fputs("usage: nib_settings_capture <png> <width px> <height px> <dpi> <theme> "
                   "<focused row|-1> <scroll px> <lsp 0|1>\n",
                   stderr);
        return 2;
    }
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        return 1;
    }
    const int result = capture(arguments);
    CoUninitialize();
    return result;
}
