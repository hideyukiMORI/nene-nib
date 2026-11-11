// Markdown プレビューの描画を offscreen（WIC の bitmap）に描いて PNG に落とす確かめの道具
// （ADR 0151 の決定 18・#784）。製品の旗は足さず、窓・device・swap chain を作らない。
// 描く経路は製品と同じ PreviewTextLayouts・PreviewBlockTops・PreviewPainter・PreviewBrushes で、
// 描画先の DPI は core::reference_dpi（座標は物理画素・本文と同じ）。
// ローカルの画像（施主決定 D79・ADR 0155 の追補 62〜65・#1439）は、製品と同じ
// PreviewImages の決まりで頼まれた順に adapter の読み（image_file_bytes）と復号
// （decoded_image）を同じスレッドで通して持ち（ワーカーは使わない・COM の STA を始める前に
// 読む）、製品と同じ PreviewBitmaps で描く。
//
//   nib_preview_capture --in <md> --out <png> [--width <DIP>] [--height <DIP>] [--dpi <96|144>]
//                       [--theme <name>] [--font-size <pt>] [--scroll <DIP>] [--font <family>]
#include "CaptureOptions.hpp"

#include "DevicePixels.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "FontSize.hpp"
#include "ImageDecode.hpp"
#include "ImageFileRead.hpp"
#include "ImageFormat.hpp"
#include "ImageOutcome.hpp"
#include "MarkdownParse.hpp"
#include "PreviewBitmaps.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewBrushes.hpp"
#include "PreviewImageWindow.hpp"
#include "PreviewImages.hpp"
#include "PreviewPainter.hpp"
#include "PreviewScene.hpp"
#include "PreviewTextLayouts.hpp"
#include "PreviewTextStyle.hpp"
#include "PreviewViewport.hpp"
#include "ThemeCatalog.hpp"
#include "ThemeName.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <d2d1.h>
#include <d2d1_1.h>
#include <fstream>
#include <iterator>
#include <memory>
#include <objbase.h>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <wincodec.h>
#include <wrl/client.h>

namespace
{
namespace core = nenenib::core;
namespace win32 = nenenib::ui::win32;
namespace app = nenenib::application;
namespace adapters = nenenib::adapters::win32;
using Document = std::expected<core::MarkdownDocument, core::MarkdownFailure>;
using Images = std::shared_ptr<const app::PreviewImages>;
using nenenib::tests::preview::CaptureOptions;
using Bitmap = Microsoft::WRL::ComPtr<IWICBitmap>;
using Imaging = Microsoft::WRL::ComPtr<IWICImagingFactory>;
using Target = Microsoft::WRL::ComPtr<ID2D1RenderTarget>;

constexpr float default_width = 720.0F;
constexpr float default_height = 900.0F;

[[nodiscard]] std::optional<float> number_of(std::string_view text)
{
    float value = 0;
    const auto *end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    return error == std::errc{} && stop == end ? std::optional<float>{value} : std::nullopt;
}

[[nodiscard]] bool assign(CaptureOptions &options, std::string_view name, std::string_view value)
{
    const std::array<std::pair<std::string_view, std::string *>, 4> texts{{
        {"--in", &options.input},
        {"--out", &options.output},
        {"--theme", &options.theme},
        {"--font", &options.font},
    }};
    const std::array<std::pair<std::string_view, float *>, 5> numbers{{
        {"--width", &options.width},
        {"--height", &options.height},
        {"--dpi", &options.dpi},
        {"--font-size", &options.points},
        {"--scroll", &options.scroll},
    }};
    for (const auto &[key, field] : texts)
    {
        if (key == name)
        {
            *field = value;
            return true;
        }
    }
    const auto parsed = number_of(value);
    for (const auto &[key, field] : numbers)
    {
        if (key == name && parsed.has_value())
        {
            *field = parsed.value();
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::optional<CaptureOptions> options_of(std::span<char *> arguments)
{
    CaptureOptions options{{},
                           {},
                           "ubuntu-aubergine",
                           std::string(core::default_editor_settings().font_family.text()),
                           default_width,
                           default_height,
                           static_cast<float>(core::reference_dpi),
                           core::FontSize::default_points,
                           0.0F};
    if (arguments.size() % 2 == 0)
    {
        return std::nullopt;
    }
    for (std::size_t index = 1; index + 1 < arguments.size(); index += 2)
    {
        if (!assign(options, arguments[index], arguments[index + 1]))
        {
            return std::nullopt;
        }
    }
    const bool complete = !options.input.empty() && !options.output.empty() && options.width >= 1 &&
                          options.height >= 1 && options.dpi >= 1;
    return complete ? std::optional<CaptureOptions>{options} : std::nullopt;
}

[[nodiscard]] std::optional<std::string> read_text(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return std::nullopt;
    }
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

[[nodiscard]] std::int32_t pixels_of(float dips, std::uint32_t dpi)
{
    return core::to_pixels(static_cast<std::int32_t>(dips), dpi);
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

[[nodiscard]] std::optional<core::Theme> theme_of(const core::ThemeCatalog &catalog,
                                                  std::string_view name)
{
    const auto parsed = core::ThemeName::parse(name);
    if (!parsed)
    {
        return std::nullopt;
    }
    const auto found = catalog.find(parsed.value());
    return found ? std::optional<core::Theme>{found.value().view()} : std::nullopt;
}

// 1 枚を読んで復号する（adapter の 2 つの関数・失敗は閉じた enum）。
[[nodiscard]] app::ImageOutcome outcome_of(const app::ImageRequest &request)
{
    auto bytes = adapters::image_file_bytes(request.path);
    if (!bytes)
    {
        return app::ImageOutcome{request.ticket, std::unexpected(bytes.error())};
    }
    const auto head =
        std::string_view(bytes.value())
            .substr(0, std::min(bytes.value().size(), core::image_format_probe_bytes));
    return app::ImageOutcome{request.ticket, adapters::decoded_image(std::span<char>(bytes.value()),
                                                                     core::image_format_of(head))};
}

// 文書の画像を製品と同じ決まりで読む（文書の全部を見える範囲として、頼めなくなるまで 1 枚ずつ）。
// 文書の場所は input（ドライブの絶対パス・区切りは \ に揃える）。画像の節が無ければ null。
[[nodiscard]] Images images_of(const core::MarkdownDocument &document, std::string input)
{
    if (document.images.empty())
    {
        return nullptr;
    }
    std::ranges::replace(input, '/', '\\');
    const auto base = core::FilePath::parse(input);
    auto images = std::make_shared<app::PreviewImages>(app::PreviewImages::of(
        document, base ? std::optional<core::FilePath>{base.value()} : std::nullopt, nullptr));
    const app::PreviewImageWindow window{0, document.blocks.size()};
    std::uint64_t tickets = 0;
    for (auto request = images->claim(document, window, true, tickets); request.has_value();
         request = images->claim(document, window, true, tickets))
    {
        static_cast<void>(images->received(outcome_of(request.value())));
    }
    std::printf("preview: %zu images, %zu bytes held\n", images->count(), images->held_bytes());
    return images;
}

// 読めた画像の bitmap を target に作る（製品の PreviewPane::hold_bitmaps と同じ）。
[[nodiscard]] bool hold_all(ID2D1RenderTarget &target, const core::MarkdownDocument &document,
                            const app::PreviewImages *images, win32::PreviewBitmaps &bitmaps)
{
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> context;
    if (images == nullptr)
    {
        return true;
    }
    if (FAILED(target.QueryInterface(IID_PPV_ARGS(&context))))
    {
        return false;
    }
    bitmaps.begin(*context.Get());
    for (std::size_t ordinal = 0; ordinal < document.images.size(); ++ordinal)
    {
        const auto pixels = images->pixels_of(ordinal);
        if (pixels != nullptr && FAILED(bitmaps.hold(*context.Get(), pixels)))
        {
            return false;
        }
    }
    return true;
}

// 描いて画素を bitmap に残す。失敗は 1 行言って false。
[[nodiscard]] bool paint(ID2D1RenderTarget &target, const core::MarkdownDocument &document,
                         const CaptureOptions &options, const std::pair<core::Theme, Images> &look)
{
    const auto &[theme, images] = look;
    const win32::PreviewTextStyle style{options.font, options.points,
                                        static_cast<std::uint32_t>(options.dpi)};
    const core::LayoutRect frame{0, 0, pixels_of(options.width, style.dpi),
                                 pixels_of(options.height, style.dpi)};
    win32::PreviewTextLayouts layouts;
    win32::PreviewBrushes brushes;
    const auto tops = win32::PreviewBlockTops::measure(
        document, layouts, style,
        win32::PreviewViewport{0, static_cast<float>(frame.right),
                               static_cast<float>(frame.bottom) +
                                   win32::preview_pixels(options.scroll, style.dpi),
                               images.get()});
    win32::PreviewBitmaps bitmaps;
    if (!tops || FAILED(brushes.prepare(target, theme)) ||
        !hold_all(target, document, images.get(), bitmaps))
    {
        std::fputs("preview: measuring or brushes failed\n", stderr);
        return false;
    }
    std::optional<win32::RenderFailure> failure;
    win32::PreviewPainter<ID2D1RenderTarget> painter(target, layouts, brushes, failure);
    target.BeginDraw();
    painter.paint(win32::PreviewScene{document, tops.value(), style, frame,
                                      win32::preview_pixels(options.scroll, style.dpi),
                                      std::nullopt, images.get(), &bitmaps});
    const bool drawn = SUCCEEDED(target.EndDraw()) && !failure.has_value();
    std::printf("preview: %zu blocks, %zu measured, %zu layouts held, %zu created\n",
                document.blocks.size(), tops.value().size(), layouts.held(), layouts.created());
    return drawn;
}

[[nodiscard]] Document document_of(const CaptureOptions &options)
{
    const auto text = read_text(options.input);
    return text ? core::parse_markdown(text.value(), core::MarkdownStop::never())
                : Document(std::unexpected(core::MarkdownFailure::parser_rejected));
}

[[nodiscard]] int capture(const CaptureOptions &options, const Document &document,
                          const Images &images)
{
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = theme_of(catalog, options.theme);
    Imaging imaging;
    Bitmap bitmap;
    Target target;
    const auto dpi = static_cast<std::uint32_t>(options.dpi);
    const core::LayoutRect area{0, 0, pixels_of(options.width, dpi),
                                pixels_of(options.height, dpi)};
    if (!document || !theme ||
        FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&imaging))) ||
        FAILED(make_target(*imaging.Get(), area, bitmap, target)))
    {
        std::fputs("preview: input, theme or target is unavailable\n", stderr);
        return 1;
    }
    if (!paint(*target.Get(), document.value(), options, std::pair{theme.value(), images}) ||
        FAILED(save_png(*imaging.Get(), *bitmap.Get(), options.output)))
    {
        return 1;
    }
    std::printf("preview: wrote %s (%d x %d px)\n", options.output.c_str(), area.right,
                area.bottom);
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    const auto options = options_of(std::span(argv, static_cast<std::size_t>(argc)));
    if (!options)
    {
        std::fputs("usage: nib_preview_capture --in <md> --out <png> [--width <DIP>] "
                   "[--height <DIP>] [--dpi <n>] [--theme <name>] [--font-size <pt>] "
                   "[--scroll <DIP>] [--font <family>]\n",
                   stderr);
        return 2;
    }
    // 画像の復号は呼んだスレッドで MTA を始めて解くので、STA を始める前に読む。
    const auto document = document_of(options.value());
    const auto images = document ? images_of(document.value(), options.value().input) : nullptr;
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        return 1;
    }
    const int result = capture(options.value(), document, images);
    CoUninitialize();
    return result;
}
