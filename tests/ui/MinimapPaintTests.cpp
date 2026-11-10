// scope `--minimap`（ADR 0171 の決定 4・7・Issue #1017 の段 1）。
// MinimapPicture を WIC の描画先で描き、次を確かめる。
// - bitmap を経た画素が、同じ frame を直接描いた画素と 1 画素も違わない。
// - 同じ行の 2 frame 目は行を描かず DrawBitmap 1 回。
// - 編集の帯だけを描き直す（同じ行の数・行が増える・減る）。
// - 前の frame を描いていなければ窓ごと描く。
// - テーマと面の幅（DPI）が変われば作り直す。
#include "WindowChecks.hpp"

#include "LayoutRect.hpp"
#include "LineNumber.hpp"
#include "MinimapBand.hpp"
#include "MinimapBlock.hpp"
#include "MinimapFrame.hpp"
#include "MinimapPen.hpp"
#include "MinimapPicture.hpp"
#include "MinimapRows.hpp"
#include "MinimapWindow.hpp"
#include "SyntaxRole.hpp"
#include "Theme.hpp"
#include "ThemeCatalog.hpp"
#include "ThemeName.hpp"

#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <memory>
#include <optional>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

namespace nenenib::tests::ui
{
namespace
{
namespace application = nenenib::application;
namespace core = nenenib::core;
namespace win32 = nenenib::ui::win32;

constexpr UINT target_width = 200;
constexpr UINT target_height = 320;
constexpr std::size_t window_rows = 120;
constexpr core::LayoutRect panel{100, 10, 180, 300};

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
    constexpr UINT stride = target_width * 4;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(stride) * target_height);
    if (FAILED(bitmap.CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data())))
    {
        return {};
    }
    return pixels;
}

// 行 index の塊（行ごとに違う長さと役割）。seed を変えると同じ行が別の中身になる。
[[nodiscard]] core::MinimapRow row_of(std::size_t index, std::size_t seed)
{
    const auto width = static_cast<std::uint16_t>((index * 7 + seed * 13) % 40 + 1);
    const auto gap = static_cast<std::uint16_t>(width + 1 + (index % 3));
    const auto role = static_cast<core::SyntaxRole>((index + seed) % 10);
    if (index % 9 == 4)
    {
        return {};
    }
    return core::MinimapRow{
        core::MinimapBlock{static_cast<std::uint16_t>(index % 8), width, std::nullopt},
        core::MinimapBlock{gap, static_cast<std::uint16_t>(30 + index % 70), role}};
}

[[nodiscard]] std::shared_ptr<application::MinimapRows> rows_of(std::size_t count)
{
    auto rows = std::make_shared<application::MinimapRows>(
        application::MinimapRows{core::MinimapWindow{core::LineNumber{1}, count}, {}, 1});
    for (std::size_t index = 0; index < count; ++index)
    {
        rows->rows.push_back(row_of(index, 0));
    }
    return rows;
}

[[nodiscard]] application::MinimapFrame
frame_of(const std::shared_ptr<application::MinimapRows> &rows, std::size_t top, std::uint64_t base,
         std::optional<core::MinimapBand> band)
{
    return application::MinimapFrame{
        rows, core::LineNumber{top}, core::LineNumber{top + 10}, 30, base, band};
}

// 描く先と brush の組。paint は 1 frame を地を消してから描き、画素を返す。
class Canvas final
{
  public:
    Canvas() : target_(wic_target(bitmap_))
    {
        if (target_ != nullptr &&
            FAILED(target_->CreateSolidColorBrush(D2D1::ColorF(0.0F, 0.0F, 0.0F), &brush_)))
        {
            brush_.Reset();
        }
    }

    [[nodiscard]] bool ready() const noexcept
    {
        return target_ != nullptr && brush_ != nullptr;
    }

    template <typename Draw> [[nodiscard]] std::vector<std::uint8_t> paint(Draw &&draw)
    {
        target_->BeginDraw();
        target_->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 1.0F));
        const bool drawn = draw(*target_.Get(), *brush_.Get());
        if (FAILED(target_->EndDraw()) || !drawn)
        {
            return {};
        }
        return pixels_of(*bitmap_.Get());
    }

    // 同じ frame を直接描いた画素（比べる相手）。
    [[nodiscard]] std::vector<std::uint8_t> direct(const application::MinimapFrame &frame,
                                                   const win32::MinimapColors &colors,
                                                   const core::LayoutRect &area = panel)
    {
        win32::MinimapPicture picture;
        return paint(
            [&](ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush)
            {
                picture.paint_directly(win32::MinimapPen{target, brush, colors}, frame, area);
                return true;
            });
    }

    // picture の bitmap を経て描いた画素。
    [[nodiscard]] std::vector<std::uint8_t> held(win32::MinimapPicture &picture,
                                                 const application::MinimapFrame &frame,
                                                 const win32::MinimapColors &colors,
                                                 const core::LayoutRect &area = panel)
    {
        return paint(
            [&](ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush)
            {
                return picture.paint(win32::MinimapPen{target, brush, colors}, frame, area)
                    .has_value();
            });
    }

  private:
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap_;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
};

[[nodiscard]] std::optional<win32::MinimapColors> colors_of(std::string_view name)
{
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse(name).value());
    if (!theme.has_value())
    {
        return std::nullopt;
    }
    const auto view = theme.value().view();
    return win32::minimap_colors(view.ui, view.body);
}

// bitmap を経た画素と直接の画素・2 frame 目の使い回し・面の最上段のずれ。
void check_held_picture(Canvas &canvas, Canvas &reference, const win32::MinimapColors &colors)
{
    win32::MinimapPicture picture;
    const auto rows = rows_of(window_rows);
    const auto first = frame_of(rows, 1, 0, std::nullopt);
    const auto through = canvas.held(picture, first, colors);
    expect(!through.empty() && through == reference.direct(first, colors),
           "the held minimap picture has the same pixels as painting directly");
    expect(picture.pictures() == 1 && picture.copies() == 1 &&
               picture.painted_rows() == window_rows,
           "the first frame draws the whole window into one bitmap");
    const auto again = canvas.held(picture, frame_of(rows, 1, 1, std::nullopt), colors);
    expect(again == through && picture.painted_rows() == window_rows && picture.copies() == 2,
           "the same rows draw no row and copy the bitmap once");
    const auto lower = frame_of(rows, 40, 1, std::nullopt);
    const auto scrolled = canvas.held(picture, lower, colors);
    expect(!scrolled.empty() && scrolled == reference.direct(lower, colors) &&
               picture.painted_rows() == window_rows,
           "a lower panel top copies another part of the same bitmap");
}

// 編集の帯: 同じ行の数・行が増える・減る。どれも直接の画素と同じで、描いた行は帯だけ。
void check_bands(Canvas &canvas, Canvas &reference, const win32::MinimapColors &colors)
{
    win32::MinimapPicture picture;
    auto rows = rows_of(window_rows);
    static_cast<void>(canvas.held(picture, frame_of(rows, 1, 0, std::nullopt), colors));
    const std::size_t whole = picture.painted_rows();
    rows->rows.at(5) = row_of(5, 3);
    rows->serial = 2;
    const auto typed = frame_of(rows, 1, 1, core::MinimapBand{5, 1, 1});
    const auto after_typing = canvas.held(picture, typed, colors);
    expect(!after_typing.empty() && after_typing == reference.direct(typed, colors) &&
               picture.painted_rows() == whole + 1 && picture.shifts() == 0,
           "a band of one row redraws that row only");
    rows->rows.at(7) = row_of(7, 5);
    rows->rows.insert(rows->rows.begin() + 8, row_of(8, 5));
    rows->window.rows = rows->rows.size();
    rows->serial = 3;
    const auto entered = frame_of(rows, 1, 2, core::MinimapBand{7, 1, 2});
    const auto after_enter = canvas.held(picture, entered, colors);
    expect(!after_enter.empty() && after_enter == reference.direct(entered, colors) &&
               picture.painted_rows() == whole + 3 && picture.shifts() == 1,
           "a row that becomes two shifts the rows below and redraws two rows");
    rows->rows.erase(rows->rows.begin() + 20, rows->rows.begin() + 22);
    rows->rows.at(20) = row_of(20, 7);
    rows->window.rows = rows->rows.size();
    rows->serial = 4;
    const auto joined = frame_of(rows, 1, 3, core::MinimapBand{20, 3, 1});
    const auto after_join = canvas.held(picture, joined, colors);
    expect(!after_join.empty() && after_join == reference.direct(joined, colors) &&
               picture.painted_rows() == whole + 4 && picture.shifts() == 2,
           "three rows that become one shift the rows up and clear the old tail");
    rows->rows.at(30) = row_of(30, 9);
    rows->serial = 6;
    const auto skipped = frame_of(rows, 1, 5, core::MinimapBand{30, 1, 1});
    const auto after_skip = canvas.held(picture, skipped, colors);
    expect(!after_skip.empty() && after_skip == reference.direct(skipped, colors) &&
               picture.painted_rows() == whole + 4 + rows->rows.size(),
           "a picture that is not the band's base draws the whole window");
}

// テーマが変われば窓ごと描き直し、面の幅（DPI）が変われば bitmap を作り直す（決定 4）。
void check_rebuilds(Canvas &canvas, Canvas &reference, const win32::MinimapColors &colors,
                    const win32::MinimapColors &other)
{
    win32::MinimapPicture picture;
    const auto rows = rows_of(window_rows);
    const auto frame = frame_of(rows, 1, 0, std::nullopt);
    static_cast<void>(canvas.held(picture, frame, colors));
    const auto themed = canvas.held(picture, frame_of(rows, 1, 1, std::nullopt), other);
    expect(!themed.empty() && themed == reference.direct(frame, other) &&
               picture.painted_rows() == 2 * window_rows && picture.pictures() == 1,
           "another theme draws the window again into the same bitmap");
    const core::LayoutRect wider{20, 10, 180, 300};
    const auto widened = canvas.held(picture, frame_of(rows, 1, 1, std::nullopt), other, wider);
    expect(!widened.empty() && widened == reference.direct(frame, other, wider) &&
               picture.painted_rows() == 3 * window_rows && picture.pictures() == 2,
           "a wider panel makes a new bitmap and draws the window again");
}
} // namespace

void verify_minimap_paint()
{
    Canvas canvas;
    Canvas reference;
    const auto colors = colors_of("ubuntu-aubergine");
    const auto other = colors_of("solarized-light");
    if (!canvas.ready() || !reference.ready() || !colors.has_value() || !other.has_value())
    {
        expect(false, "the minimap checks prepare two WIC targets and two themes");
        return;
    }
    check_held_picture(canvas, reference, colors.value());
    check_bands(canvas, reference, colors.value());
    check_rebuilds(canvas, reference, colors.value(), other.value());
}
} // namespace nenenib::tests::ui
