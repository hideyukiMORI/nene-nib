// Markdown プレビューの画像の段 3（施主決定 D79・ADR 0155 の追補 62〜65・#1439）の契約。
// 本物の DirectWrite と WIC の bitmap の描画先（ソフトウェアの Direct2D）で、製品の PreviewPane を
// 通して描き、読めた画像の画素が 1 画素も違わずに出ること・枠より広い画像は枠の本文の幅に縮めて
// 縦横比を保つこと・読む前と失敗は「[画像: alt]」の文字のまま（画像の状態が無い描画と同じ画素）・
// 読めたら高さを測り直して後ろの要素が下がること・画像の状態が替われば描き直し同じなら使い回す
// こと・bitmap の保持と捨て方（画像の無い文書・release・放たれた画素・描く先の替わり・32 MiB の
// 上限）・画像の上の点は箱の前か後ろを数える。
// 字体は Segoe UI（Windows の既定で入っている）で、無い機械では測らない（QLT-013）。
#include "DirectWriteFactory.hpp"
#include "WindowChecks.hpp"

#include "FilePath.hpp"
#include "ImageGeometry.hpp"
#include "ImageLimits.hpp"
#include "ImageOutcome.hpp"
#include "ImagePixels.hpp"
#include "ImageSize.hpp"
#include "LayoutRect.hpp"
#include "MarkdownDocument.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "Offset.hpp"
#include "PreviewBitmaps.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewBrushes.hpp"
#include "PreviewFrame.hpp"
#include "PreviewImageWindow.hpp"
#include "PreviewImages.hpp"
#include "PreviewInlineImages.hpp"
#include "PreviewMetrics.hpp"
#include "PreviewPainter.hpp"
#include "PreviewPane.hpp"
#include "PreviewPoint.hpp"
#include "PreviewScene.hpp"
#include "PreviewSelectionText.hpp"
#include "PreviewTaskBox.hpp"
#include "PreviewTextLayouts.hpp"
#include "PreviewTextStyle.hpp"
#include "PreviewViewport.hpp"
#include "RenderFailure.hpp"
#include "Theme.hpp"
#include "ThemeCatalog.hpp"
#include "ThemeChoice.hpp"
#include "ThemeName.hpp"

#include <algorithm>
#include <combaseapi.h>
#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <d2d1_1.h>
#include <dxgi.h>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

namespace nenenib::tests::ui
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
namespace win32 = nenenib::ui::win32;
using core::ImageSize;

constexpr UINT target_width = 400;
constexpr UINT target_height = 200;
constexpr UINT stride = target_width * 4;
constexpr core::LayoutRect area{0, 0, static_cast<std::int32_t>(target_width),
                                static_cast<std::int32_t>(target_height)};
constexpr std::string_view readme = "C:\\docs\\readme.md";
// 枠の本文の幅（物理画素）: 枠の幅から左右の余白（16 DIP ずつ）を引いた幅。
constexpr std::uint32_t body_dips = target_width - 32;

[[nodiscard]] win32::PreviewTextStyle style_of()
{
    return win32::PreviewTextStyle{"Segoe UI", 13.5F, 96};
}

// メモリの bitmap のソフトウェアの描画先（PreviewLayoutTests と同じ作り方）。
[[nodiscard]] Microsoft::WRL::ComPtr<ID2D1RenderTarget>
wic_target(Microsoft::WRL::ComPtr<IWICBitmap> &bitmap)
{
    Microsoft::WRL::ComPtr<ID2D1Factory> factory;
    Microsoft::WRL::ComPtr<IWICImagingFactory> imaging;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> target;
    const auto properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf())) ||
        FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&imaging))) ||
        FAILED(imaging->CreateBitmap(target_width, target_height, GUID_WICPixelFormat32bppPBGRA,
                                     WICBitmapCacheOnLoad, &bitmap)) ||
        FAILED(factory->CreateWicBitmapRenderTarget(bitmap.Get(), properties, &target)))
    {
        return nullptr;
    }
    return target;
}

[[nodiscard]] std::vector<std::uint8_t> pixels_of(IWICBitmap &bitmap)
{
    std::vector<std::uint8_t> pixels(std::size_t{stride} * target_height);
    if (FAILED(bitmap.CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data())))
    {
        return {};
    }
    return pixels;
}

// 位置ごとに色の違う不透明の画素（B は x・G は y・R は x + y で決まる）。
[[nodiscard]] std::shared_ptr<const app::ImagePixels> pattern(ImageSize size)
{
    std::vector<std::uint8_t> bgra;
    bgra.reserve(std::size_t{size.width} * size.height * core::image_pixel_bytes);
    for (std::uint32_t y = 0; y < size.height; ++y)
    {
        for (std::uint32_t x = 0; x < size.width; ++x)
        {
            bgra.push_back(static_cast<std::uint8_t>(10U + x * 20U));
            bgra.push_back(static_cast<std::uint8_t>(200U - y * 30U));
            bgra.push_back(static_cast<std::uint8_t>(60U + (x + y) * 7U));
            bgra.push_back(255U);
        }
    }
    return std::make_shared<const app::ImagePixels>(app::ImagePixels{size, size, std::move(bgra)});
}

// 1 色の不透明の画素（original の大きさから復号した大きさ）。
[[nodiscard]] std::shared_ptr<const app::ImagePixels> solid(ImageSize original)
{
    const auto size = core::image_decoded_size(original);
    std::vector<std::uint8_t> bgra;
    bgra.reserve(std::size_t{size.width} * size.height * core::image_pixel_bytes);
    for (std::size_t index = 0; index < std::size_t{size.width} * size.height; ++index)
    {
        bgra.insert(bgra.end(), {40U, 220U, 30U, 255U});
    }
    return std::make_shared<const app::ImagePixels>(
        app::ImagePixels{original, size, std::move(bgra)});
}

[[nodiscard]] std::shared_ptr<const core::MarkdownDocument> document_of(std::string_view text)
{
    return std::make_shared<const core::MarkdownDocument>(
        core::parse_markdown(text, core::MarkdownStop::never()).value());
}

// 文書の画像の状態。loaded の順に、頼まれた画像へ画素を置く（失敗の nullptr は corrupt）。
[[nodiscard]] std::shared_ptr<const app::PreviewImages>
images_of(const core::MarkdownDocument &document,
          const std::vector<std::shared_ptr<const app::ImagePixels>> &loaded)
{
    auto images = std::make_shared<app::PreviewImages>(
        app::PreviewImages::of(document, core::FilePath::parse(readme).value(), nullptr));
    const app::PreviewImageWindow window{0, document.blocks.size()};
    std::uint64_t tickets = 0;
    for (const auto &pixels : loaded)
    {
        const auto request = images->claim(document, window, true, tickets);
        if (!request.has_value())
        {
            break;
        }
        const auto ticket = request.value().ticket;
        static_cast<void>(images->received(
            pixels == nullptr
                ? app::ImageOutcome{ticket, std::unexpected(app::ImageFailure::corrupt)}
                : app::ImageOutcome{ticket, pixels}));
    }
    return images;
}

// 描画先 1 つとテーマ。
class Canvas final
{
  public:
    Canvas()
        : theme_(core::ThemeCatalog::builtins()
                     .find(core::ThemeName::parse("ubuntu-aubergine").value())
                     .value())
    {
        target_ = wic_target(bitmap_);
    }
    [[nodiscard]] bool ready() const noexcept
    {
        return target_ != nullptr;
    }
    // frame を pane で描いた画素（失敗したら空）。
    [[nodiscard]] std::vector<std::uint8_t> paint(win32::PreviewPane &pane,
                                                  const app::PreviewFrame &frame)
    {
        if (!ready() || !pane.prepare(*target_.Get(), theme_.view()))
        {
            return {};
        }
        target_->BeginDraw();
        target_->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
        const auto painted = pane.paint(*target_.Get(), frame, style_of(), area);
        if (FAILED(target_->EndDraw()) || !painted)
        {
            return {};
        }
        return pixels_of(*bitmap_.Get());
    }

    // 描く先の device context（試験の WIC の描く先も持つ）。
    [[nodiscard]] Microsoft::WRL::ComPtr<ID2D1DeviceContext> context() const
    {
        Microsoft::WRL::ComPtr<ID2D1DeviceContext> context;
        if (target_ == nullptr || FAILED(target_.As(&context)))
        {
            return nullptr;
        }
        return context;
    }

    // draw() を透明に塗った描く先の上で通した画素（失敗したら空）。
    template <typename Draw> [[nodiscard]] std::vector<std::uint8_t> painted(Draw &&draw)
    {
        if (!ready())
        {
            return {};
        }
        target_->BeginDraw();
        target_->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
        draw();
        if (FAILED(target_->EndDraw()))
        {
            return {};
        }
        return pixels_of(*bitmap_.Get());
    }

  private:
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap_;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> target_;
    core::ThemeChoice theme_;
};

[[nodiscard]] app::PreviewFrame frame_of(std::shared_ptr<const core::MarkdownDocument> document,
                                         std::shared_ptr<const app::PreviewImages> images)
{
    return app::PreviewFrame{std::move(document), core::Offset{0}, std::nullopt, std::nullopt,
                             std::move(images)};
}

// drawn の (left, top) から source の全部の画素がそのまま並ぶか。
[[nodiscard]] bool same_at(const std::vector<std::uint8_t> &drawn, const app::ImagePixels &source,
                           UINT left, UINT top)
{
    const auto width = source.size.width;
    for (UINT y = 0; y < source.size.height; ++y)
    {
        const auto row = drawn.begin() + static_cast<std::ptrdiff_t>((top + y) * stride + left * 4);
        const auto from = source.bgra.begin() + static_cast<std::ptrdiff_t>(y * width * 4);
        if (!std::equal(from, from + static_cast<std::ptrdiff_t>(width * 4), row))
        {
            return false;
        }
    }
    return true;
}

// 描いた画素の中で、source の全部の画素がそのまま並ぶ左上の位置（無ければ nullopt）。
[[nodiscard]] std::optional<std::pair<UINT, UINT>>
find_exact(const std::vector<std::uint8_t> &drawn, const app::ImagePixels &source)
{
    if (drawn.empty() || source.size.width > target_width || source.size.height > target_height)
    {
        return std::nullopt;
    }
    const UINT columns = target_width - source.size.width + 1;
    const UINT rows = target_height - source.size.height + 1;
    for (UINT index = 0; index < columns * rows; ++index)
    {
        if (same_at(drawn, source, index % columns, index / columns))
        {
            return std::pair{index % columns, index / columns};
        }
    }
    return std::nullopt;
}

// 描いた画素の中で color と同じ画素の数と、それを囲む矩形の幅と高さ。
[[nodiscard]] std::pair<std::size_t, ImageSize> coloured(const std::vector<std::uint8_t> &drawn,
                                                         const std::vector<std::uint8_t> &color)
{
    std::size_t count = 0;
    UINT left = target_width;
    UINT top = target_height;
    UINT right = 0;
    UINT bottom = 0;
    for (UINT index = 0; index < target_width * target_height && !drawn.empty(); ++index)
    {
        const UINT x = index % target_width;
        const UINT y = index / target_width;
        if (std::equal(color.begin(), color.end(),
                       drawn.begin() + static_cast<std::ptrdiff_t>(std::size_t{index} * 4)))
        {
            ++count;
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x + 1);
            bottom = std::max(bottom, y + 1);
        }
    }
    return {count, right > left ? ImageSize{right - left, bottom - top} : ImageSize{0, 0}};
}

// 読めた画像はその画素のまま（1 画素 = 1 DIP・96 DPI で拡大縮小しない）出る。同じ段落の 2 枚目も
// 出る。bitmap は 2 つ作る。
void check_loaded_pixels(Canvas &canvas)
{
    const auto document = document_of("Intro\n\n![one](a.png) and ![two](b.png)\n\nafter\n");
    const auto first = pattern(ImageSize{6, 4});
    const auto second = pattern(ImageSize{3, 5});
    win32::PreviewPane pane;
    const auto drawn =
        canvas.paint(pane, frame_of(document, images_of(*document, {first, second})));
    const auto one = find_exact(drawn, *first);
    const auto two = find_exact(drawn, *second);
    expect(one.has_value() && two.has_value() && one.value().first < two.value().first,
           "#1439 loaded images are drawn with their own pixels in the text flow");
    expect(pane.bitmaps().count() == 2 && pane.bitmaps().created() == 2 &&
               pane.bitmaps().bytes() == first->bgra.size() + second->bgra.size(),
           "#1439 one bitmap is made for each loaded image");
    // 画像の上の点は、画像の前か後ろ（画像は 1 つの単位で、選択の中には入らない）。
    const auto point = one.has_value()
                           ? pane.point_at(static_cast<std::int32_t>(one.value().first + 2),
                                           static_cast<std::int32_t>(one.value().second + 1))
                           : std::nullopt;
    const auto &image = document->images.front();
    expect(
        point.has_value() && point.value().block == image.block &&
            (point.value().offset == image.begin.value || point.value().offset == image.end.value),
        "#1439 a point on an image is before or after the whole image");
}

// 読む前・失敗・画像の状態が無いときは、どれも「[画像: alt]」の文字だけの同じ画素。
void check_unloaded_stays_text(Canvas &canvas)
{
    const auto document = document_of("![one](a.png)\n\nafter ![web](https://x/y.png)\n");
    win32::PreviewPane plain_pane;
    win32::PreviewPane waiting_pane;
    win32::PreviewPane failed_pane;
    const auto plain = canvas.paint(plain_pane, frame_of(document, nullptr));
    const auto waiting = canvas.paint(waiting_pane, frame_of(document, images_of(*document, {})));
    const auto failed =
        canvas.paint(failed_pane, frame_of(document, images_of(*document, {nullptr})));
    expect(!plain.empty() && plain == waiting && plain == failed,
           "#1439 images not read yet and failed images stay the [image: alt] text");
    expect(waiting_pane.bitmaps().count() == 0 && failed_pane.bitmaps().count() == 0,
           "#1439 no bitmap is made for an image that is not loaded");
}

// 枠より広い画像は枠の本文の幅に縮めて縦横比を保つ（core::image_placement の大きさ）。
void check_scaled_to_frame(Canvas &canvas)
{
    const auto document = document_of("![wide](wide.png)\n");
    const auto wide = solid(ImageSize{1000, 50});
    win32::PreviewPane pane;
    const auto drawn = canvas.paint(pane, frame_of(document, images_of(*document, {wide})));
    const auto placed = core::image_placement(wide->original, body_dips);
    const auto [count, box] = coloured(drawn, {40U, 220U, 30U, 255U});
    expect(placed.width == body_dips && box == placed &&
               count == std::size_t{placed.width} * placed.height,
           "#1439 a wide image is shrunk to the frame width and keeps its aspect ratio");
}

// 読めた画像の高さで測り直し、後ろの要素が画像の高さぶん下がる。
void check_remeasured(Canvas &canvas)
{
    const auto document = document_of("![tall](tall.png)\n\nafter\n");
    const auto tall = solid(ImageSize{30, 150});
    const auto images = images_of(*document, {tall});
    win32::PreviewTextLayouts layouts;
    const auto style = style_of();
    const auto width = static_cast<float>(target_width);
    const auto height = std::numeric_limits<float>::max();
    const auto text = win32::PreviewBlockTops::measure(*document, layouts, style,
                                                       win32::PreviewViewport{0, width, height});
    const auto loaded = win32::PreviewBlockTops::measure(
        *document, layouts, style, win32::PreviewViewport{0, width, height, images.get()});
    expect(text.has_value() && loaded.has_value() &&
               loaded.value().bottom(0) - loaded.value().top(0) >= 150.0F &&
               loaded.value().top(1) - text.value().top(1) >=
                   150.0F - (text.value().bottom(0) - text.value().top(0)),
           "#1439 a loaded image is measured at its height and the next block moves down");
    win32::PreviewPane pane;
    const auto drawn = canvas.paint(pane, frame_of(document, images));
    const auto [count, box] = coloured(drawn, {40U, 220U, 30U, 255U});
    expect(box == ImageSize{30, 150} && count == 30U * 150U,
           "#1439 a tall image is drawn at its own size");
}

// 画像の状態が替われば描き直し（読めた画像が出る）、同じ状態なら保持の絵を使い回す。
void check_picture_key(Canvas &canvas)
{
    const auto document = document_of("![one](a.png)\n");
    const auto first = pattern(ImageSize{6, 4});
    win32::PreviewPane pane;
    const auto before = canvas.paint(pane, frame_of(document, images_of(*document, {})));
    const auto images = images_of(*document, {first});
    const auto after = canvas.paint(pane, frame_of(document, images));
    const auto again = canvas.paint(pane, frame_of(document, images));
    expect(!before.empty() && !find_exact(before, *first).has_value() &&
               find_exact(after, *first).has_value() && after == again,
           "#1439 a loaded image is drawn when the image state changes");
    expect(pane.painted() == 2 && pane.reused() == 1 && pane.bitmaps().created() == 1,
           "#1439 the same image state reuses the held picture without making bitmaps");
}

// bitmap の捨て方: 画像の無い文書・release・放たれた画素・描く先の替わり。
void check_released(Canvas &canvas)
{
    const auto document = document_of("![one](a.png)\n");
    win32::PreviewPane pane;
    auto first = pattern(ImageSize{6, 4});
    auto images = images_of(*document, {first});
    static_cast<void>(canvas.paint(pane, frame_of(document, images)));
    const bool held = pane.bitmaps().count() == 1;
    static_cast<void>(canvas.paint(pane, frame_of(document_of("plain\n"), nullptr)));
    expect(held && pane.bitmaps().count() == 0 && pane.bitmaps().bytes() == 0,
           "#1439 a document without images releases the image bitmaps");
    static_cast<void>(canvas.paint(pane, frame_of(document, images)));
    pane.release();
    expect(pane.bitmaps().count() == 0 && pane.point_at(10, 10) == std::nullopt,
           "#1439 release drops the bitmaps and what was measured");
    static_cast<void>(canvas.paint(pane, frame_of(document, images)));
    // application が画素を捨てた（別の状態に替わった）後は、次の描画で bitmap も捨てる。
    images = nullptr;
    first = nullptr;
    const auto second = pattern(ImageSize{3, 5});
    static_cast<void>(canvas.paint(pane, frame_of(document, images_of(*document, {second}))));
    expect(pane.bitmaps().count() == 1 && pane.bitmaps().bytes() == second->bgra.size(),
           "#1439 a bitmap whose pixels were dropped by the application is released");
    // 描く先（device context）が替われば全部捨てて、新しい描く先に作り直す（製品では device lost の
    // 作り直しで renderer ごと新しくなる）。
    Canvas other;
    win32::PreviewBitmaps bitmaps;
    const auto here = canvas.context();
    const auto there = other.context();
    if (here == nullptr || there == nullptr)
    {
        expect(false, "#1439 the software targets have device contexts");
        return;
    }
    bitmaps.begin(*here.Get());
    const bool made =
        SUCCEEDED(bitmaps.hold(*here.Get(), second)) && bitmaps.find(second.get()) != nullptr;
    bitmaps.begin(*there.Get());
    const bool dropped = bitmaps.count() == 0;
    const bool remade = made && dropped && SUCCEEDED(bitmaps.hold(*there.Get(), second)) &&
                        bitmaps.find(second.get()) != nullptr;
    expect(remade && bitmaps.count() == 1 && bitmaps.created() == 2,
           "#1439 another device context drops the bitmaps and makes them again");
}

// 保持は 32 MiB まで: 16 MiB の画素 3 枚を順に描くと、今の描画で使わない最も古い 1 枚を捨てる。
void check_held_limit(Canvas &canvas)
{
    const auto document = document_of("![big](big.png)\n");
    std::vector<std::shared_ptr<const app::ImagePixels>> kept;
    win32::PreviewPane pane;
    for (int index = 0; index < 3; ++index)
    {
        kept.push_back(solid(ImageSize{2048, 2048}));
        static_cast<void>(
            canvas.paint(pane, frame_of(document, images_of(*document, {kept.back()}))));
    }
    expect(pane.bitmaps().count() == 2 &&
               pane.bitmaps().bytes() == core::preview_image_held_byte_limit,
           "#1439 the image bitmaps are held up to 32 MiB and the oldest goes first");
}

// 作れない bitmap と引けない位置（ADR 0155 の追補 67）: device lost だけが失敗で、GPU の記憶の
// 不足などは画像を描かないだけ。作ってはいけない画素（大きさ 0・列の長さが合わない・device の上限を
// 超える）は CreateBitmap に渡さずに断り、持っている bitmap は使えるまま。
void check_unmade_bitmaps(Canvas &canvas)
{
    expect(win32::preview_bitmap_lost(D2DERR_RECREATE_TARGET) &&
               win32::preview_bitmap_lost(DXGI_ERROR_DEVICE_REMOVED) &&
               !win32::preview_bitmap_lost(E_OUTOFMEMORY) &&
               !win32::preview_bitmap_lost(E_INVALIDARG),
           "#1439 only a lost device fails the preview when an image bitmap is not made");
    const auto context = canvas.context();
    if (context == nullptr)
    {
        expect(false, "#1439 the software target has a device context");
        return;
    }
    const auto good = pattern(ImageSize{3, 5});
    const auto limit = context->GetMaximumBitmapSize();
    const auto empty = std::make_shared<const app::ImagePixels>(
        app::ImagePixels{ImageSize{0, 0}, ImageSize{0, 0}, {}});
    const auto short_row = std::make_shared<const app::ImagePixels>(
        app::ImagePixels{ImageSize{4, 4}, ImageSize{4, 4}, std::vector<std::uint8_t>(12)});
    const auto beyond = std::make_shared<const app::ImagePixels>(app::ImagePixels{
        ImageSize{limit + 1, 1}, ImageSize{limit + 1, 1},
        std::vector<std::uint8_t>(std::size_t{limit + 1} * core::image_pixel_bytes)});
    win32::PreviewBitmaps bitmaps;
    bitmaps.begin(*context.Get());
    const bool made = SUCCEEDED(bitmaps.hold(*context.Get(), good));
    const bool refused = FAILED(bitmaps.hold(*context.Get(), empty)) &&
                         FAILED(bitmaps.hold(*context.Get(), short_row)) &&
                         FAILED(bitmaps.hold(*context.Get(), beyond));
    expect(made && refused && bitmaps.count() == 1 && bitmaps.created() == 1 &&
               bitmaps.find(good.get()) != nullptr && bitmaps.find(empty.get()) == nullptr,
           "#1439 pixels that cannot become a bitmap are refused without touching the held ones");
}

// bitmap の無い画像は箱が空のまま描かれ、frame は失敗にならず、ほかの画像と本文は出る（作れなかった
// 画像の形・PreviewPane の hold_bitmaps は device lost でない失敗を飛ばす）。
void check_missing_bitmap_paints(Canvas &canvas)
{
    const auto document = document_of("![one](a.png) and ![two](b.png)\n\nafter\n");
    const auto first = pattern(ImageSize{6, 4});
    const auto second = pattern(ImageSize{3, 5});
    const auto images = images_of(*document, {first, second});
    const auto context = canvas.context();
    win32::PreviewTextLayouts layouts;
    const auto style = style_of();
    const auto tops = win32::PreviewBlockTops::measure(
        *document, layouts, style,
        win32::PreviewViewport{0, static_cast<float>(target_width),
                               static_cast<float>(target_height), images.get()});
    win32::PreviewBitmaps bitmaps;
    win32::PreviewBrushes brushes;
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    if (context == nullptr || !tops || !theme ||
        FAILED(brushes.prepare(*context.Get(), theme.value().view())))
    {
        expect(false, "#1439 the painter checks have a target, the heights and the brushes");
        return;
    }
    bitmaps.begin(*context.Get());
    // 1 枚目は作れなかった形（持たない）、2 枚目だけを持つ。
    const bool held = SUCCEEDED(bitmaps.hold(*context.Get(), second));
    std::optional<win32::RenderFailure> failure;
    win32::PreviewPainter<ID2D1RenderTarget> painter(*context.Get(), layouts, brushes, failure);
    const auto drawn = canvas.painted(
        [&]
        {
            painter.paint(win32::PreviewScene{*document, tops.value(), style, area, 0.0F,
                                              std::nullopt, images.get(), &bitmaps});
        });
    expect(held && !failure.has_value() && !drawn.empty() &&
               find_exact(drawn, *second).has_value() && !find_exact(drawn, *first).has_value(),
           "#1439 an image without a bitmap leaves its box empty and the frame still paints");
}

// 箱のあるタスクの項目は、チェックボックスを 1 行目の文字の baseline の下がりだけ下げる（描く所と
// 押す所が同じ矩形・ADR 0155 の追補 68）。折り返す項目でも 1
// 行目に揃え、箱の無い項目は今のまま（下がり 0）。
void check_task_box_follows_image()
{
    const auto document =
        document_of("- [ ] ![tall](t.png) a note long enough to wrap onto a second line in the "
                    "frame of four hundred pixels\n- [ ] plain\n");
    const auto tall = solid(ImageSize{30, 60});
    const auto images = images_of(*document, {tall});
    win32::PreviewTextLayouts layouts;
    const auto style = style_of();
    const auto width = static_cast<float>(target_width);
    const auto tops = win32::PreviewBlockTops::measure(
        *document, layouts, style,
        win32::PreviewViewport{0, width, std::numeric_limits<float>::max(), images.get()});
    if (!tops)
    {
        expect(false, "#1439 the task items are measured");
        return;
    }
    const auto &measured = tops.value();
    const auto text_width =
        win32::preview_text_width(document->blocks.at(0), width, measured.indent(0), style.dpi);
    const auto shift = layouts.first_line_shift(
        document->blocks.at(0), text_width, style,
        win32::preview_inline_images(*document, images.get(), 0,
                                     win32::preview_dips_of(text_width, style.dpi)));
    const auto expected_box =
        win32::preview_task_box(measured.indent(0), measured.top(0) + shift.value_or(0.0F), style);
    const auto plain = win32::preview_task_box(measured.indent(1), measured.top(1), style);
    const auto boxed = measured.task_box(0);
    const auto unboxed = measured.task_box(1);
    expect(shift.has_value() && shift.value_or(0.0F) > 30.0F && boxed.has_value() &&
               unboxed.has_value() && boxed.value().square == expected_box.square &&
               unboxed.value().square == plain.square,
           "#1439 a task box moves down to the first line of an item with a loaded image");
}
} // namespace

void verify_preview_images()
{
    const auto factory = directwrite_factory();
    if (!factory || !installed(*factory.Get(), L"Segoe UI"))
    {
        unmeasured("#1439 preview images need Segoe UI");
        return;
    }
    Canvas canvas;
    expect(canvas.ready(), "#1439 the software target and the theme are made");
    if (!canvas.ready())
    {
        return;
    }
    check_loaded_pixels(canvas);
    check_unloaded_stays_text(canvas);
    check_scaled_to_frame(canvas);
    check_remeasured(canvas);
    check_picture_key(canvas);
    check_released(canvas);
    check_held_limit(canvas);
    check_unmade_bitmaps(canvas);
    check_missing_bitmap_paints(canvas);
    check_task_box_follows_image();
}
} // namespace nenenib::tests::ui
