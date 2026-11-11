// scope `--minimap`（ADR 0171 の決定 4・7・Issue #1017 の段 1）。
// MinimapPicture を WIC の描画先で描き、次を確かめる。
// - bitmap を経た画素が、同じ frame を直接描いた画素と 1 画素も違わない。
// - 同じ行の 2 frame 目は行を描かず DrawBitmap 1 回。
// - 編集の帯だけを描き直す（同じ行の数・行が増える・減る）。
// - 前の frame を描いていなければ窓ごと描く。
// - テーマと面の幅（DPI）が変われば作り直す。
// - 行の増減の無い帯は命令を流さない（Flush は行をずらすときだけ）。
// - どの core::SyntaxRole の値の塊も core::syntax_color の色で描く（色の表の位置を数えない）。
#include "WindowChecks.hpp"

#include "AutosaveId.hpp"
#include "Edit.hpp"
#include "Language.hpp"
#include "LayoutRect.hpp"
#include "LineNumber.hpp"
#include "Minimap.hpp"
#include "MinimapBand.hpp"
#include "MinimapBlock.hpp"
#include "MinimapCache.hpp"
#include "MinimapFlush.hpp"
#include "MinimapFrame.hpp"
#include "MinimapPen.hpp"
#include "MinimapPicture.hpp"
#include "MinimapRows.hpp"
#include "MinimapSource.hpp"
#include "MinimapWindow.hpp"
#include "Offset.hpp"
#include "SyntaxColor.hpp"
#include "SyntaxPalette.hpp"
#include "SyntaxRole.hpp"
#include "TextBuffer.hpp"
#include "Theme.hpp"
#include "ThemeCatalog.hpp"
#include "ThemeName.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <d2d1.h>
#include <dxgi.h>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
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

// 面の UI の色と本文の構文の色（MinimapPen の 2 つの色の組）。
using Ink = std::pair<win32::MinimapColors, core::SyntaxPalette>;

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
                                                   const Ink &colors,
                                                   const core::LayoutRect &area = panel)
    {
        win32::MinimapPicture picture;
        return paint(
            [&](ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush)
            {
                picture.paint_directly(
                    win32::MinimapPen{target, brush, colors.first, colors.second}, frame, area);
                return true;
            });
    }

    // picture の bitmap を経て描いた画素。
    [[nodiscard]] std::vector<std::uint8_t> held(win32::MinimapPicture &picture,
                                                 const application::MinimapFrame &frame,
                                                 const Ink &colors,
                                                 const core::LayoutRect &area = panel)
    {
        return paint(
            [&](ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush)
            {
                return picture
                    .paint(win32::MinimapPen{target, brush, colors.first, colors.second}, frame,
                           area)
                    .has_value();
            });
    }

  private:
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap_;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
};

[[nodiscard]] std::optional<Ink> colors_of(std::string_view name)
{
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse(name).value());
    if (!theme.has_value())
    {
        return std::nullopt;
    }
    const auto view = theme.value().view();
    return Ink{win32::minimap_colors(view.ui), view.body};
}

// bitmap を経た画素と直接の画素・2 frame 目の使い回し・面の最上段のずれ。
void check_held_picture(Canvas &canvas, Canvas &reference, const Ink &colors)
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
void check_bands(Canvas &canvas, Canvas &reference, const Ink &colors)
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
               picture.painted_rows() == whole + 1 && picture.shifts() == 0 &&
               picture.flushes() == 0,
           "a band of one row redraws that row only and flushes nothing");
    rows->rows.at(7) = row_of(7, 5);
    rows->rows.insert(rows->rows.begin() + 8, row_of(8, 5));
    rows->window.rows = rows->rows.size();
    rows->serial = 3;
    const auto entered = frame_of(rows, 1, 2, core::MinimapBand{7, 1, 2});
    const auto after_enter = canvas.held(picture, entered, colors);
    expect(!after_enter.empty() && after_enter == reference.direct(entered, colors) &&
               picture.painted_rows() == whole + 3 && picture.shifts() == 1 &&
               picture.flushes() == 1,
           "a row that becomes two flushes once, shifts the rows below and redraws two rows");
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
void check_rebuilds(Canvas &canvas, Canvas &reference, const Ink &colors, const Ink &other)
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
// 画素の列の (x, y) の色（BGRA・不透明）。
[[nodiscard]] core::RgbColor pixel_at(const std::vector<std::uint8_t> &pixels, UINT x, UINT y)
{
    const std::size_t at = (static_cast<std::size_t>(y) * target_width + x) * 4;
    return core::RgbColor{pixels.at(at + 2), pixels.at(at + 1), pixels.at(at)};
}

// どの役割の塊も core::syntax_color（default の無い switch の 1 本）の色で描く。役割が増えれば
// そこでコンパイルが落ち、表の位置がずれて例外や別の色になることはない（レビュー M1）。
void check_every_role(Canvas &canvas, const Ink &colors)
{
    constexpr auto last = std::to_underlying(core::SyntaxRole::warning);
    auto rows = std::make_shared<application::MinimapRows>(application::MinimapRows{
        core::MinimapWindow{core::LineNumber{1}, std::size_t{last} + 1}, {}, 1});
    for (std::size_t value = 0; value <= last; ++value)
    {
        rows->rows.push_back(
            core::MinimapRow{core::MinimapBlock{0, 10, static_cast<core::SyntaxRole>(value)}});
    }
    rows->rows.push_back(core::MinimapRow{core::MinimapBlock{0, 10, std::nullopt}});
    // 見えている範囲の面は役割の行より下（first_visible = 40）に置き、色に重ねない。
    const application::MinimapFrame frame{rows, core::LineNumber{1}, core::LineNumber{40}, 1,
                                          0,    std::nullopt};
    const auto pixels = canvas.direct(frame, colors);
    if (pixels.empty())
    {
        expect(false, "the role rows are painted");
        return;
    }
    bool matched = true;
    for (std::size_t value = 0; value <= last; ++value)
    {
        const auto y =
            static_cast<UINT>(panel.top) + static_cast<UINT>(value * core::minimap_row_pixels);
        matched =
            matched && pixel_at(pixels, static_cast<UINT>(panel.left) + 5, y) ==
                           core::syntax_color(colors.second, static_cast<core::SyntaxRole>(value));
    }
    const auto muted_y =
        static_cast<UINT>(panel.top) + static_cast<UINT>((last + 1) * core::minimap_row_pixels);
    expect(matched &&
               pixel_at(pixels, static_cast<UINT>(panel.left) + 5, muted_y) == colors.first.at(2),
           "every syntax role paints in its syntax_color and a block without one in muted");
}

// ミニマップの失敗の読み（#1449）: 成功は kept、device lost は tag に依らず device_lost、ほかの
// 失敗は minimap_tag ならミニマップ・それ以外は frame。bitmap の上限を超える面は bitmap
// を作らない。
void check_failure_reading()
{
    expect(win32::minimap_flush_of(S_OK, 0) == win32::MinimapFlush::kept &&
               win32::minimap_flush_of(E_INVALIDARG, win32::minimap_tag) ==
                   win32::MinimapFlush::minimap &&
               win32::minimap_flush_of(E_INVALIDARG, 0) == win32::MinimapFlush::frame &&
               win32::minimap_flush_of(D2DERR_WRONG_STATE, win32::minimap_tag) ==
                   win32::MinimapFlush::minimap &&
               win32::minimap_flush_of(D2DERR_RECREATE_TARGET, win32::minimap_tag) ==
                   win32::MinimapFlush::device_lost &&
               win32::minimap_flush_of(DXGI_ERROR_DEVICE_REMOVED, 0) ==
                   win32::MinimapFlush::device_lost,
           "a failure tagged by the minimap is the minimap's and device lost stays device lost");
    // 5,000 行の窓は 5,120 行刻み = 10,240 画素。FL 11 の 16,384 には入り、FL 10 の 8,192・FL 9 の
    // 4,096 には入らない（直接描く）。幅 0 も作らない。
    expect(win32::minimap_bitmap_fits(80, 5'000, 16'384) &&
               !win32::minimap_bitmap_fits(80, 5'000, 8'192) &&
               !win32::minimap_bitmap_fits(80, 3'000, 4'096) &&
               win32::minimap_bitmap_fits(80, 2'000, 4'096) &&
               !win32::minimap_bitmap_fits(0, 10, 16'384) &&
               !win32::minimap_bitmap_fits(20'000, 10, 16'384),
           "a minimap bitmap over the device's maximum bitmap size is not made");
}

// bitmap の道を捨てた絵は直接描き（DrawBitmap を数えない）、画素は直接の画素と同じ。
void check_abandoned_bitmap(Canvas &canvas, Canvas &reference, const Ink &colors)
{
    win32::MinimapPicture picture;
    const auto rows = rows_of(window_rows);
    static_cast<void>(canvas.held(picture, frame_of(rows, 1, 0, std::nullopt), colors));
    picture.abandon_bitmap();
    const auto frame = frame_of(rows, 20, 1, std::nullopt);
    const auto direct = canvas.held(picture, frame, colors);
    expect(picture.bitmap_abandoned() && picture.copies() == 1 && !direct.empty() &&
               direct == reference.direct(frame, colors),
           "an abandoned minimap bitmap paints directly with the same pixels");
}

// ---------------------------------------------------------------- 固定 seed の帯の列（#1449）

using Random = std::mt19937_64;

constexpr std::array<std::string_view, 9> edit_texts{"x",  "\n",       "\n\n", "/*", "*/",
                                                     "あ", "int y;\n", "",     "\t"};
// 面の寸法: 普通・幅 0・高さ 0・高さ 1 画素・狭い（bitmap を作り直す）・低い。
constexpr std::array<core::LayoutRect, 6> random_panels{{{100, 10, 180, 300},
                                                         {100, 10, 100, 300},
                                                         {100, 10, 180, 10},
                                                         {100, 10, 180, 11},
                                                         {120, 10, 180, 300},
                                                         {100, 10, 180, 60}}};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] core::TextBuffer numbered_text(std::size_t count)
{
    std::string text;
    for (std::size_t line = 1; line <= count; ++line)
    {
        text += line % 11 == 0 ? "/* note\n" : "";
        text += "int value_" + std::to_string(line) + " = " + std::to_string(line) + ";\n";
        text += line % 11 == 0 ? "*/\n" : "";
    }
    auto made = core::TextBuffer::from_utf8(text);
    return made.has_value() ? std::move(made).value() : core::TextBuffer::empty();
}

// 本文の任意の位置の置き換え（UTF-8 の途中も含む）を 1 つ当てて覚えに知らせる。
void edit_randomly(application::MinimapCache &cache, core::TextBuffer &text, Random &random)
{
    const std::size_t size = text.size_bytes();
    const core::Offset at{below(random, size + 1)};
    const core::Offset end{std::min(size, at.value + below(random, 3) * below(random, 40))};
    const std::string inserted(edit_texts.at(below(random, edit_texts.size())));
    const core::Edit edit{at, text.text_range(at, end), inserted, at};
    auto after = text.replaced(at, end, inserted);
    cache.edited(text, after, edit, application::AutosaveId{1});
    text = std::move(after);
}

// controller の add_minimap と同じ手順で frame を作る。
[[nodiscard]] application::MinimapFrame framed_randomly(application::MinimapCache &cache,
                                                        const core::TextBuffer &text,
                                                        core::LineNumber first_visible,
                                                        std::size_t panel_rows)
{
    const application::MinimapSource source{application::AutosaveId{1}, core::Language::cpp, true};
    const std::size_t total = text.line_count();
    const auto top = core::minimap_top(first_visible, 30, total, panel_rows);
    const auto window = core::minimap_window(cache.window_of(source), top, panel_rows, total);
    if (!cache.holds(source, window))
    {
        cache.rebuild(text, source, window);
    }
    return cache.framed(top, first_visible, 30);
}

// 1 本の走りの状態: 行の塊の覚え・絵・本文・見えている最初の行・乱数。
using Run = std::tuple<application::MinimapCache, win32::MinimapPicture, core::TextBuffer,
                       std::size_t, Random>;
// 描く先と比べる相手の組。
using Canvases = std::pair<Canvas &, Canvas &>;

// 0〜2 個の編集・ときどき送り・面の寸法を選んで frame を作る。
[[nodiscard]] std::pair<application::MinimapFrame, core::LayoutRect> next_frame(Run &run)
{
    auto &[cache, picture, text, first, random] = run;
    for (std::size_t edits = below(random, 3); edits > 0; --edits)
    {
        edit_randomly(cache, text, random);
    }
    first = below(random, 4) == 0 ? 1 + below(random, text.line_count()) : first;
    first = std::min(first, text.line_count());
    const auto area = random_panels.at(below(random, random_panels.size()));
    const auto rows = static_cast<std::size_t>(std::max(core::height_of(area), 0) / 2);
    return {framed_randomly(cache, text, core::LineNumber{first}, rows), area};
}

// 1 歩。4 回に 3 回は描いて直接の画素と比べる。違うか描画が失敗したら偽。
[[nodiscard]] bool painted_step(Canvases canvases, const std::array<Ink, 2> &inks, Run &run)
{
    const auto [frame, area] = next_frame(run);
    auto &random = std::get<Random>(run);
    if (below(random, 4) == 0)
    {
        return true;
    }
    const Ink &ink = inks.at(below(random, 8) == 0 ? 1 : 0);
    const auto through =
        canvases.first.held(std::get<win32::MinimapPicture>(run), frame, ink, area);
    return !through.empty() && through == canvases.second.direct(frame, ink, area);
}

[[nodiscard]] std::size_t broken_seed(Canvases canvases, const std::array<Ink, 2> &inks,
                                      std::uint64_t seed)
{
    Run run{application::MinimapCache{}, win32::MinimapPicture{},
            numbered_text(seed == 3 ? 4'600 : 200), std::size_t{1}, Random{seed}};
    // 窓より長い文書（seed 3）は窓ごとの描き直しが重いので歩を減らす（Debug で数秒に収める）。
    const std::size_t steps = seed == 3 ? 40 : 200;
    for (std::size_t step = 0; step < steps; ++step)
    {
        if (!painted_step(canvases, inks, run))
        {
            std::fprintf(stderr, "minimap paint: seed %llu step %zu differs\n",
                         static_cast<unsigned long long>(seed), step);
            return 1;
        }
    }
    return 0;
}

// 編集・送り・面の寸法とテーマの切り替えを混ぜ、bitmap を経た画素が直接の画素と同じで、描画が
// 失敗しない（EndDraw が成功する）。frame を作っても描かない歩を挟む。
void check_random_frames(Canvas &canvas, Canvas &reference, const Ink &colors, const Ink &other)
{
    const std::array<Ink, 2> inks{colors, other};
    std::size_t broken = 0;
    for (std::uint64_t seed = 1; seed <= 3; ++seed)
    {
        broken += broken_seed(Canvases{canvas, reference}, inks, seed);
    }
    expect(broken == 0,
           "random bands, panels and themes paint the same pixels as painting directly");
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
    check_every_role(canvas, colors.value());
    check_random_frames(canvas, reference, colors.value(), other.value());
    check_failure_reading();
    check_abandoned_bitmap(canvas, reference, colors.value());
}
} // namespace nenenib::tests::ui
