// Markdown プレビューの差し替えの費用の probe（#818・ADR 0155 の決定 7）。CTest に登録しない。
// 決定的に作った Markdown を parse_markdown で読み、PreviewTextLayouts ＋ PreviewBlockTops::measure
// で錨から 1 枠ぶん測る。製品の PreviewPane は通さない（nib_preview_capture と同じ土台）。
// 時刻は Win32TimingAdapter の節目だけで取る（ARC-007）: parse と measure をそれぞれ
// probe_started / probe_finished の 1 対で挟み、節目の
// JSON（<json>.timing.json）を読み戻して差を書く。
// --long-paragraph（#1477）は行内コードの多い 1 つの長い段落（424 span で 23.7 KB・span
// の数は引数）の 文書を WIC の描画先に製品と同じ PreviewPainter で描く。最初の 1 回（layout
// を作って色を置く）と、 錨を 0 と 1 で動かしながらの描き直し repaints
// 回を別の節目の対で測り、描き直しの間に layout を 作った数・SetDrawingEffect・HitTestTextRange
// の数を書く（保持した layout を汚さないことの数）。 続けて配色を変えた後の 1
// 回（同じ描画先で別のテーマ・D59 の配色のプレビュー）と枠の幅を 1 px 広げた後の 1
// 回（幅のドラッグ）も測る（#1485）。
//
//   nib_preview_probe <bytes> <head|middle|tail> <json の出力先>
//   nib_preview_probe --long-paragraph <spans> <repaints> <json の出力先>
//   nib_preview_probe --paragraph-stages <link|code|image|picture|long> <spans> <json の出力先>
#include "PreviewProbeOutcome.hpp"
#include "Sha256.hpp"

#include "Appearance.hpp"
#include "DevicePixels.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "FontSize.hpp"
#include "ImageGeometry.hpp"
#include "ImageLimits.hpp"
#include "ImageOutcome.hpp"
#include "ImagePixels.hpp"
#include "ImageSize.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "Milestone.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewBrushes.hpp"
#include "PreviewImageWindow.hpp"
#include "PreviewImages.hpp"
#include "PreviewMetrics.hpp"
#include "PreviewPainter.hpp"
#include "PreviewScene.hpp"
#include "PreviewTextLayouts.hpp"
#include "PreviewTextStyle.hpp"
#include "PreviewViewport.hpp"
#include "Utf16.hpp"
#include "Win32TimingAdapter.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <d2d1.h>
#include <format>
#include <fstream>
#include <memory>
#include <objbase.h>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

namespace
{
namespace core = nenenib::core;
namespace win32 = nenenib::ui::win32;
using nenenib::tests::preview::PreviewProbeOutcome;

constexpr std::size_t subsections = 4U;
constexpr std::size_t paragraphs = 3U;
constexpr std::int32_t viewport_width = 900;
constexpr std::int32_t viewport_height = 640;
constexpr std::uint32_t viewport_dpi = 120;
constexpr std::size_t tail_distance = 10;
constexpr std::size_t timing_marks = 4;
constexpr std::int64_t microseconds_per_millisecond = 1000;
constexpr std::array<std::string_view, 3> anchors{"head", "middle", "tail"};

void append_subsection(std::string &text, std::size_t section, std::size_t subsection)
{
    text += std::format("## Subsection {}.{}\n\n", section, subsection);
    for (std::size_t paragraph = 0; paragraph < paragraphs; ++paragraph)
    {
        text += std::format("This is paragraph {} with *emphasis*, **strong text**, `inline code` "
                            "and a [link](https://example.com/{}/{}/{}).\n",
                            paragraph, section, subsection, paragraph);
        text += "Second line of the paragraph continues the sentence with plain words to make "
                "it longer.\n";
        text += "Third line has an autolink <https://example.org/x> and some more "
                "_underscored emphasis_ here.\n\n";
    }
    text += "- first item with **bold**\n- second item with `code`\n  - nested item with "
            "[ref](/a/b)\n- third item\n\n";
    text += "1. ordered one\n2. ordered two\n3. ordered three\n\n";
    text += "> A block quote line with *emphasis*.\n> Another quoted line.\n\n";
    text += "```c\nint main(void) {\n    return 0;\n}\n```\n\n";
    text += "    indented code line one\n    indented code line two\n\n";
    text += "---\n\n";
}

// tests/performance/MarkdownProbe.cpp の markdown_input と同じ規則（共有できない理由:
// preview_tests は performance_tests に依存できない・eng/architecture.json）。節を bytes まで
// 繰り返し、bytes で切って最後の改行まで。16 MiB で同じバイト列になる。
[[nodiscard]] std::string markdown_text(std::size_t bytes)
{
    std::string text;
    text.reserve(bytes + 4096U);
    for (std::size_t section = 0; text.size() < bytes; ++section)
    {
        text += std::format("# Section {}\n\n", section);
        for (std::size_t subsection = 0; subsection < subsections; ++subsection)
        {
            append_subsection(text, section, subsection);
        }
    }
    text.resize(bytes);
    text.resize(text.rfind('\n') + 1U);
    return text;
}

[[nodiscard]] std::optional<std::size_t> size_of(std::string_view text)
{
    std::size_t value = 0;
    const auto *end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    return error == std::errc{} && stop == end && value > 0 ? std::optional<std::size_t>{value}
                                                            : std::nullopt;
}

[[nodiscard]] std::optional<std::size_t> anchor_of(std::string_view name)
{
    for (std::size_t index = 0; index < anchors.size(); ++index)
    {
        if (anchors[index] == name)
        {
            return index;
        }
    }
    return std::nullopt;
}

// head は 0、middle はブロックの数の半分、tail は最後の 10 個前（足りなければ 0）。
[[nodiscard]] std::size_t first_block(std::size_t anchor, std::size_t blocks)
{
    if (anchor == 0)
    {
        return 0;
    }
    if (anchor == 1)
    {
        return blocks / 2;
    }
    return blocks > tail_distance ? blocks - tail_distance : 0;
}

[[nodiscard]] float pixels_of(std::int32_t dips)
{
    return static_cast<float>(core::to_pixels(dips, viewport_dpi));
}

// 節目を積むのはこの関数の中だけ。戻るときに timing のデストラクタが節目の JSON を書く。
[[nodiscard]] std::optional<PreviewProbeOutcome>
timed_run(const std::string &text, std::size_t anchor, const std::wstring &timing_path)
{
    nenenib::adapters::win32::Win32TimingAdapter timing;
    timing.bind(timing_path);
    timing.mark(core::Milestone::probe_started);
    const auto document = core::parse_markdown(text, core::MarkdownStop::never());
    timing.mark(core::Milestone::probe_finished);
    if (!document)
    {
        return std::nullopt;
    }
    const auto blocks = document.value().blocks.size();
    const auto first = first_block(anchor, blocks);
    const win32::PreviewTextStyle style{
        std::string(core::default_editor_settings().font_family.text()),
        core::FontSize::default_points, viewport_dpi};
    win32::PreviewTextLayouts layouts;
    timing.mark(core::Milestone::probe_started);
    const auto tops = win32::PreviewBlockTops::measure(
        document.value(), layouts, style,
        win32::PreviewViewport{first, pixels_of(viewport_width), pixels_of(viewport_height)});
    timing.mark(core::Milestone::probe_finished);
    if (!tops)
    {
        return std::nullopt;
    }
    return PreviewProbeOutcome{blocks, first, tops.value().size() - tops.value().first(),
                               layouts.created()};
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

// 節目の JSON の qpcMicroseconds を順に読む。
[[nodiscard]] std::vector<std::int64_t> mark_times(std::string_view report)
{
    constexpr std::string_view key = "\"qpcMicroseconds\": ";
    std::vector<std::int64_t> times;
    for (auto at = report.find(key); at != std::string_view::npos; at = report.find(key, at))
    {
        at += key.size();
        std::int64_t value = 0;
        const auto [stop, error] =
            std::from_chars(report.data() + at, report.data() + report.size(), value);
        if (error != std::errc{})
        {
            return {};
        }
        times.push_back(value);
        at = static_cast<std::size_t>(stop - report.data());
    }
    return times;
}

[[nodiscard]] std::string milliseconds_text(std::int64_t microseconds)
{
    return std::format("{}.{:03d}", microseconds / microseconds_per_millisecond,
                       microseconds % microseconds_per_millisecond);
}

[[nodiscard]] std::string report_of(const std::string &text, std::string_view anchor,
                                    const PreviewProbeOutcome &outcome,
                                    std::span<const std::int64_t> times)
{
    return std::format(
        "{{\"schema\":1,\"probe\":\"nib_preview_probe\",\"bytes\":{},\"sha256\":\"{}\","
        "\"blocks\":{},\"anchor\":\"{}\",\"first\":{},\"measured\":{},\"parseMs\":{},"
        "\"measureMs\":{},\"layoutsCreated\":{},\"viewport\":{{\"widthDip\":{},\"heightDip\":{},"
        "\"dpi\":{}}},\"commit\":\"{}\",\"dirty\":{},\"configuration\":\"{}\"}}\n",
        text.size(), nenenib::tests::preview::sha256_hex(text), outcome.blocks, anchor,
        outcome.first, outcome.measured, milliseconds_text(times[1] - times[0]),
        milliseconds_text(times[3] - times[2]), outcome.layouts_created, viewport_width,
        viewport_height, viewport_dpi, NIB_PROBE_COMMIT, NIB_PROBE_DIRTY, NIB_PROBE_CONFIGURATION);
}

// 行内コードの多い長い段落（span 個の行内コードと同じ数のリンク）を見出しと短い段落で挟む。
[[nodiscard]] std::string long_paragraph_text(std::size_t spans)
{
    std::string text = "# Long paragraph\n\n";
    for (std::size_t index = 0; index < spans; ++index)
    {
        text += std::format("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE3\x81\xAE"
                            "\xE8\xAA\xAC\xE6\x98\x8E\xE3\x82\x92\xE7\xB6\x9A"
                            "\xE3\x81\x91\xE3\x82\x8B `span_{}` [link](x/{}) ",
                            index, index);
    }
    text += "\n\n";
    for (std::size_t index = 0; index < tail_distance; ++index)
    {
        text += std::format("short paragraph {}\n\n", index);
    }
    return text;
}

// 描画先の WIC の bitmap と Direct2D のソフトウェアの描画先（DPI は core::reference_dpi）。
[[nodiscard]] Microsoft::WRL::ComPtr<ID2D1RenderTarget>
software_target(Microsoft::WRL::ComPtr<IWICBitmap> &bitmap)
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
        FAILED(imaging->CreateBitmap(static_cast<UINT>(pixels_of(viewport_width)),
                                     static_cast<UINT>(pixels_of(viewport_height)),
                                     GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad,
                                     &bitmap)) ||
        FAILED(factory->CreateWicBitmapRenderTarget(bitmap.Get(), properties, &target)))
    {
        return nullptr;
    }
    return target;
}

// 描く先と色と枠の幅に足す物理画素（幅のドラッグの 1 px・#1485・ふだんは 0）と読めた画像
// （無ければ null）。
using Canvas = std::tuple<ID2D1RenderTarget *, const win32::PreviewBrushes *, float,
                          const nenenib::application::PreviewImages *>;

// 錨 first から測って 1 回描く。失敗は false。
[[nodiscard]] bool paint_once(const Canvas &canvas, const core::MarkdownDocument &document,
                              win32::PreviewTextLayouts &layouts, std::size_t first)
{
    auto &target = *std::get<0>(canvas);
    const win32::PreviewTextStyle style{
        std::string(core::default_editor_settings().font_family.text()),
        core::FontSize::default_points, viewport_dpi};
    const auto width = pixels_of(viewport_width) + std::get<2>(canvas);
    const auto height = pixels_of(viewport_height);
    const auto tops = win32::PreviewBlockTops::measure(
        document, layouts, style,
        win32::PreviewViewport{first, width, height, std::get<3>(canvas)});
    if (!tops)
    {
        return false;
    }
    std::optional<win32::RenderFailure> failure;
    win32::PreviewPainter<ID2D1RenderTarget> painter(target, layouts, *std::get<1>(canvas),
                                                     failure);
    target.BeginDraw();
    painter.paint(win32::PreviewScene{
        document, tops.value(), style,
        core::LayoutRect{0, 0, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height)},
        0.0F, std::nullopt, std::get<3>(canvas)});
    return SUCCEEDED(target.EndDraw()) && !failure.has_value();
}

using LayoutCounts = std::tuple<std::size_t, std::size_t, std::size_t>;

[[nodiscard]] LayoutCounts counts_of(const win32::PreviewTextLayouts &layouts)
{
    return {layouts.created(), layouts.inked(), layouts.hit_tests()};
}

// --long-paragraph の段（節目の対の順・#1485）: 最初の 1 回・錨を動かしながらの描き直し・配色を
// 変えた後の 1 回（別のテーマで brush を作り直す・D59 の配色のプレビューと同じ道）・枠の幅を 1 px
// 広げた後の 1 回（幅のドラッグ）。
constexpr std::array<std::string_view, 4> long_moments{"first", "repaint", "theme", "width"};
using MomentCounts = std::array<LayoutCounts, long_moments.size()>;

// 段ごとに節目の対で挟む。返すのは各段の後の数（累計）。
[[nodiscard]] std::optional<MomentCounts> timed_paints(const core::MarkdownDocument &document,
                                                       std::size_t repaints,
                                                       const std::wstring &timing_path)
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = software_target(bitmap);
    win32::PreviewBrushes brushes;
    const auto settings = core::default_editor_settings();
    if (target == nullptr ||
        FAILED(
            brushes.prepare(*target.Get(), core::selected_theme(settings, core::Appearance::dark))))
    {
        return std::nullopt;
    }
    win32::PreviewTextLayouts layouts;
    nenenib::adapters::win32::Win32TimingAdapter timing;
    timing.bind(timing_path);
    MomentCounts counts{};
    timing.mark(core::Milestone::probe_started);
    const Canvas canvas{target.Get(), &brushes, 0.0F, nullptr};
    const Canvas widened{target.Get(), &brushes, 1.0F, nullptr};
    bool painted = paint_once(canvas, document, layouts, 1);
    timing.mark(core::Milestone::probe_finished);
    counts.at(0) = counts_of(layouts);
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < repaints && painted; ++index)
    {
        painted = paint_once(canvas, document, layouts, index % 2);
    }
    timing.mark(core::Milestone::probe_finished);
    counts.at(1) = counts_of(layouts);
    painted = painted &&
              SUCCEEDED(brushes.prepare(*target.Get(),
                                        core::selected_theme(settings, core::Appearance::light)));
    timing.mark(core::Milestone::probe_started);
    painted = painted && paint_once(canvas, document, layouts, 0);
    timing.mark(core::Milestone::probe_finished);
    counts.at(2) = counts_of(layouts);
    timing.mark(core::Milestone::probe_started);
    painted = painted && paint_once(widened, document, layouts, 0);
    timing.mark(core::Milestone::probe_finished);
    counts.at(3) = counts_of(layouts);
    if (!painted)
    {
        return std::nullopt;
    }
    return counts;
}

// sizes は span の数と描き直しの回数。段ごとの秒と、段の間に増えた数を書く。
[[nodiscard]] std::string long_report_of(const std::string &text,
                                         std::pair<std::size_t, std::size_t> sizes,
                                         const MomentCounts &counts,
                                         std::span<const std::int64_t> times)
{
    const auto &[spans, repaints] = sizes;
    std::string moments;
    for (std::size_t index = 0; index < long_moments.size(); ++index)
    {
        const auto &[created, inked, hits] = counts.at(index);
        const auto &[created_before, inked_before, hits_before] =
            index == 0 ? LayoutCounts{} : counts.at(index - 1);
        moments += std::format(",\"{}\":{{\"ms\":{},\"layoutsCreated\":{},\"drawingEffects\":{},"
                               "\"hitTests\":{}}}",
                               long_moments.at(index),
                               milliseconds_text(times[2 * index + 1] - times[2 * index]),
                               created - created_before, inked - inked_before, hits - hits_before);
    }
    return std::format("{{\"schema\":2,\"probe\":\"nib_preview_probe\",\"mode\":\"long-paragraph\","
                       "\"bytes\":{},\"sha256\":\"{}\",\"spans\":{},\"repaints\":{}{},"
                       "\"viewport\":{{\"widthDip\":{},\"heightDip\":{},\"dpi\":{}}},"
                       "\"commit\":\"{}\",\"dirty\":{},\"configuration\":\"{}\"}}\n",
                       text.size(), nenenib::tests::preview::sha256_hex(text), spans, repaints,
                       moments, viewport_width, viewport_height, viewport_dpi, NIB_PROBE_COMMIT,
                       NIB_PROBE_DIRTY, NIB_PROBE_CONFIGURATION);
}

[[nodiscard]] int long_paragraph_probe(std::size_t spans, std::size_t repaints,
                                       const std::string &output)
{
    const std::string timing_path = output + ".timing.json";
    const auto wide = core::to_utf16(timing_path);
    const std::string text = long_paragraph_text(spans);
    const auto document = core::parse_markdown(text, core::MarkdownStop::never());
    const auto counts =
        wide && document ? timed_paints(document.value(), repaints, wide.value()) : std::nullopt;
    const auto timing = read_text(timing_path);
    const auto times = timing ? mark_times(timing.value()) : std::vector<std::int64_t>{};
    if (!counts || times.size() != 2U * long_moments.size())
    {
        std::fputs("preview probe: parsing, painting or the timing file failed\n", stderr);
        return 1;
    }
    std::ofstream file(output, std::ios::binary);
    file << long_report_of(text, std::pair{spans, repaints}, counts.value(), times);
    return file ? 0 : 1;
}

// --paragraph-stages（#1485）: 空行の無い 1 段落に行内の要素が spans 個（1 行に 1 個・行は
// 改行で続く）。kind は link（`line N [link](xN.md)`）・code（`line N \`cN\``）・image
// （`line N ![i](xN.png)`・画像は読まない）・picture（`line N ![i](x.png)`・同じ経路の画像を読めた
// 状態にする・箱になるのは最初の preview_image_limit 個）・long（--long-paragraph と同じ段落）。
// 製品の道（錨 0 からの measure・最初の描画・描き直し 1 回・配色を変えた後の 1 回・枠の幅を 1 px
// 広げた後の 1 回）と、同じ段落の layout に DirectWrite を直に呼ぶ段（UTF-16 の位置の写し・
// CreateTextLayout・装飾・HitTestTextRange・SetDrawingEffect・DrawTextLayout）を節目の対で挟んで
// 秒を分ける。
constexpr std::array<std::string_view, 5> paragraph_kinds{"link", "code", "image", "picture",
                                                          "long"};
constexpr std::size_t picture_kind = 3;
constexpr std::size_t long_kind = 4;

// 文書の画像の状態で、全部の節が同じ経路 x.png を指す（1 つの entry）ものに 16 × 16 の画素を置く
// （画像の読みは通さない・ファイルに触れない）。読めるのは最初の preview_image_limit 個の節。
[[nodiscard]] std::shared_ptr<const nenenib::application::PreviewImages>
loaded_images(const core::MarkdownDocument &document)
{
    namespace app = nenenib::application;
    auto images = std::make_shared<app::PreviewImages>(app::PreviewImages::of(
        document, core::FilePath::parse("C:\\docs\\readme.md").value(), nullptr));
    std::uint64_t tickets = 0;
    const auto request =
        images->claim(document, app::PreviewImageWindow{0, document.blocks.size()}, true, tickets);
    constexpr std::uint32_t side = 16;
    const auto size = core::image_decoded_size(core::ImageSize{side, side});
    std::vector<std::uint8_t> bgra(std::size_t{size.width} * size.height * core::image_pixel_bytes,
                                   std::uint8_t{200});
    if (!request.has_value() ||
        !images->received(app::ImageOutcome{
            request.value().ticket, std::make_shared<const app::ImagePixels>(app::ImagePixels{
                                        core::ImageSize{side, side}, size, std::move(bgra)})}))
    {
        return nullptr;
    }
    return images;
}

[[nodiscard]] std::string paragraph_text(std::size_t kind, std::size_t spans)
{
    if (kind == long_kind)
    {
        return long_paragraph_text(spans);
    }
    std::string text = "# Paragraph\n\n";
    for (std::size_t index = 0; index < spans; ++index)
    {
        switch (kind)
        {
        case 0:
            text += std::format("line {} [link](x{}.md)\n", index, index);
            break;
        case 1:
            text += std::format("line {} `c{}`\n", index, index);
            break;
        case 2:
            text += std::format("line {} ![i](x{}.png)\n", index, index);
            break;
        case picture_kind:
            text += std::format("line {} ![i](x.png)\n", index);
            break;
        }
    }
    text += "\n";
    for (std::size_t index = 0; index < tail_distance; ++index)
    {
        text += std::format("short paragraph {}\n\n", index);
    }
    return text;
}

// 段の名前（節目の対の順）。
constexpr std::array<std::string_view, 12> stage_names{
    "parse",          "productMeasure", "productFirstPaint",
    "productRepaint", "productTheme",   "productWidth",
    "units",          "createPlain",    "decorate",
    "hitTests",       "drawingEffects", "draw"};

// 装飾を直に置く（ranges は spans と同じ順の UTF-16 の範囲）。
[[nodiscard]] HRESULT decorate_direct(IDWriteTextLayout &layout, const core::PreviewBlock &block,
                                      std::span<const DWRITE_TEXT_RANGE> ranges)
{
    for (std::size_t index = 0; index < block.spans.size() && index < ranges.size(); ++index)
    {
        const auto style = block.spans.at(index).style;
        const auto range = ranges[index];
        const auto result = style == core::PreviewStyle::code
                                ? layout.SetFontFamilyName(L"Consolas", range)
                            : style == core::PreviewStyle::link
                                ? layout.SetUnderline(TRUE, range)
                                : layout.SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range);
        if (range.length > 0 && FAILED(result))
        {
            return result;
        }
    }
    return S_OK;
}

// DirectWrite を直に呼ぶ段（節目は stage_names の 5 番目から）。spans の UTF-16 の範囲は先に写す。
[[nodiscard]] bool direct_stages(nenenib::adapters::win32::Win32TimingAdapter &timing,
                                 ID2D1RenderTarget &target, const core::PreviewBlock &block,
                                 ID2D1Brush *brush)
{
    const auto shown = core::preview_shown_text(block);
    const auto wide = core::to_utf16(shown);
    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    if (!wide ||
        FAILED(
            DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &factory)) ||
        FAILED(factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                         18.0F, L"", &format)))
    {
        return false;
    }
    // 製品と同じ写し（preview_unit_ranges・前から 1 回だけ数える・#1485）。
    timing.mark(core::Milestone::probe_started);
    std::vector<win32::PreviewByteRange> bytes;
    bytes.reserve(block.spans.size());
    for (const auto &span : block.spans)
    {
        bytes.emplace_back(span.begin.value, span.end.value);
    }
    const auto ranges = win32::preview_unit_ranges(shown, bytes);
    timing.mark(core::Milestone::probe_finished);
    DWRITE_TEXT_METRICS metrics{};
    const auto length = static_cast<UINT32>(wide.value().size());
    timing.mark(core::Milestone::probe_started);
    const bool made =
        SUCCEEDED(factory->CreateTextLayout(wide.value().c_str(), length, format.Get(),
                                            pixels_of(viewport_width), 1.0e7F, &layout)) &&
        SUCCEEDED(layout->GetMetrics(&metrics));
    timing.mark(core::Milestone::probe_finished);
    timing.mark(core::Milestone::probe_started);
    const bool decorated = made && SUCCEEDED(decorate_direct(*layout.Get(), block, ranges)) &&
                           SUCCEEDED(layout->GetMetrics(&metrics));
    timing.mark(core::Milestone::probe_finished);
    timing.mark(core::Milestone::probe_started);
    for (const auto &range : ranges)
    {
        UINT32 count = 0;
        std::array<DWRITE_HIT_TEST_METRICS, 4> runs{};
        (void)layout->HitTestTextRange(range.startPosition, range.length, 0.0F, 0.0F, runs.data(),
                                       static_cast<UINT32>(runs.size()), &count);
    }
    timing.mark(core::Milestone::probe_finished);
    timing.mark(core::Milestone::probe_started);
    for (const auto &range : ranges)
    {
        (void)layout->SetDrawingEffect(brush, range);
    }
    timing.mark(core::Milestone::probe_finished);
    timing.mark(core::Milestone::probe_started);
    target.BeginDraw();
    target.DrawTextLayout(D2D1::Point2F(0.0F, 0.0F), layout.Get(), brush,
                          D2D1_DRAW_TEXT_OPTIONS_NONE);
    const bool drawn = SUCCEEDED(target.EndDraw());
    timing.mark(core::Milestone::probe_finished);
    return decorated && drawn;
}

// picture の段落だけ画像を読めた状態にする（ほかは null）。
[[nodiscard]] std::shared_ptr<const nenenib::application::PreviewImages>
images_of(std::size_t kind, const core::MarkdownDocument &document)
{
    if (kind != picture_kind)
    {
        return nullptr;
    }
    return loaded_images(document);
}

// 製品の道と直の段を流す。返すのは製品の最初の measure・最初の描画・描き直し・配色を変えた後・
// 枠の幅を 1 px 広げた後の数（累計）。
[[nodiscard]] std::optional<std::array<LayoutCounts, 5>>
staged_run(std::size_t kind, const std::string &text, const std::wstring &timing_path)
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = software_target(bitmap);
    win32::PreviewBrushes brushes;
    const auto settings = core::default_editor_settings();
    if (target == nullptr ||
        FAILED(
            brushes.prepare(*target.Get(), core::selected_theme(settings, core::Appearance::dark))))
    {
        return std::nullopt;
    }
    nenenib::adapters::win32::Win32TimingAdapter timing;
    timing.bind(timing_path);
    timing.mark(core::Milestone::probe_started);
    const auto document = core::parse_markdown(text, core::MarkdownStop::never());
    timing.mark(core::Milestone::probe_finished);
    if (!document)
    {
        return std::nullopt;
    }
    const win32::PreviewTextStyle style{
        std::string(core::default_editor_settings().font_family.text()),
        core::FontSize::default_points, viewport_dpi};
    win32::PreviewTextLayouts layouts;
    const auto images = images_of(kind, document.value());
    timing.mark(core::Milestone::probe_started);
    const auto tops = win32::PreviewBlockTops::measure(
        document.value(), layouts, style,
        win32::PreviewViewport{0, pixels_of(viewport_width), pixels_of(viewport_height),
                               images.get()});
    timing.mark(core::Milestone::probe_finished);
    const auto measured = counts_of(layouts);
    const Canvas canvas{target.Get(), &brushes, 0.0F, images.get()};
    const Canvas widened{target.Get(), &brushes, 1.0F, images.get()};
    timing.mark(core::Milestone::probe_started);
    bool painted = tops.has_value() && paint_once(canvas, document.value(), layouts, 0);
    timing.mark(core::Milestone::probe_finished);
    const auto first = counts_of(layouts);
    timing.mark(core::Milestone::probe_started);
    painted = painted && paint_once(canvas, document.value(), layouts, 0);
    timing.mark(core::Milestone::probe_finished);
    const auto last = counts_of(layouts);
    painted = painted &&
              SUCCEEDED(brushes.prepare(*target.Get(),
                                        core::selected_theme(settings, core::Appearance::light)));
    timing.mark(core::Milestone::probe_started);
    painted = painted && paint_once(canvas, document.value(), layouts, 0);
    timing.mark(core::Milestone::probe_finished);
    const auto themed = counts_of(layouts);
    timing.mark(core::Milestone::probe_started);
    painted = painted && paint_once(widened, document.value(), layouts, 0);
    timing.mark(core::Milestone::probe_finished);
    const auto stretched = counts_of(layouts);
    if (!painted || document.value().blocks.size() < 2 ||
        !direct_stages(timing, *target.Get(), document.value().blocks.at(1),
                       brushes.of(win32::PreviewInk::accent)))
    {
        return std::nullopt;
    }
    return std::array{measured, first, last, themed, stretched};
}

// 段ごとの秒（"<段>Ms":<ms> を , でつなぐ）。
[[nodiscard]] std::string stage_times(std::span<const std::int64_t> times)
{
    std::string stages;
    for (std::size_t index = 0; index < stage_names.size(); ++index)
    {
        stages += std::format("{}\"{}Ms\":{}", index == 0 ? "" : ",", stage_names.at(index),
                              milliseconds_text(times[2 * index + 1] - times[2 * index]));
    }
    return stages;
}

// 製品の道の段の間に増えた数（measure は 0 からの数）。
[[nodiscard]] std::string stage_tallies(const std::array<LayoutCounts, 5> &counts)
{
    constexpr std::array<std::string_view, 5> moments{"measure", "firstPaint", "repaint", "theme",
                                                      "width"};
    std::string tallies;
    for (std::size_t index = 0; index < moments.size(); ++index)
    {
        const auto &[created, inked, hits] = counts.at(index);
        const auto &[created_before, inked_before, hits_before] =
            index == 0 ? LayoutCounts{} : counts.at(index - 1);
        tallies += std::format(",\"{}\":{{\"layoutsCreated\":{},\"drawingEffects\":{},"
                               "\"hitTests\":{}}}",
                               moments.at(index), created - created_before, inked - inked_before,
                               hits - hits_before);
    }
    return tallies;
}

[[nodiscard]] int paragraph_stages_probe(std::size_t kind, std::size_t spans,
                                         const std::string &output)
{
    const std::string timing_path = output + ".timing.json";
    const auto wide = core::to_utf16(timing_path);
    const std::string text = paragraph_text(kind, spans);
    const auto counts = wide ? staged_run(kind, text, wide.value()) : std::nullopt;
    const auto timing = read_text(timing_path);
    const auto times = timing ? mark_times(timing.value()) : std::vector<std::int64_t>{};
    if (!counts || times.size() != 2U * stage_names.size())
    {
        std::fputs("preview probe: the staged run or the timing file failed\n", stderr);
        return 1;
    }
    std::ofstream file(output, std::ios::binary);
    file << std::format("{{\"schema\":2,\"probe\":\"nib_preview_probe\",\"mode\":\"paragraph-"
                        "stages\",\"kind\":\"{}\",\"spans\":{},\"bytes\":{},{}{},"
                        "\"commit\":\"{}\",\"dirty\":{},\"configuration\":\"{}\"}}\n",
                        paragraph_kinds.at(kind), spans, text.size(), stage_times(times),
                        stage_tallies(counts.value()), NIB_PROBE_COMMIT, NIB_PROBE_DIRTY,
                        NIB_PROBE_CONFIGURATION);
    return file ? 0 : 1;
}

// --long-paragraph の引数を読んで流す（読めなければ 2）。
[[nodiscard]] int long_paragraph_main(std::span<char *> arguments)
{
    const auto spans = size_of(arguments[2]);
    const auto repaints = size_of(arguments[3]);
    if (!spans || !repaints || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        std::fputs("usage: nib_preview_probe --long-paragraph <spans> <repaints> <json>\n", stderr);
        return 2;
    }
    const int result = long_paragraph_probe(spans.value(), repaints.value(), arguments[4]);
    CoUninitialize();
    return result;
}

// --paragraph-stages の引数を読んで流す（読めなければ 2）。
[[nodiscard]] int paragraph_stages_main(std::span<char *> arguments)
{
    const auto kind = std::ranges::find(paragraph_kinds, std::string_view(arguments[2]));
    const auto spans = size_of(arguments[3]);
    if (kind == paragraph_kinds.end() || !spans ||
        FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        std::fputs("usage: nib_preview_probe --paragraph-stages <link|code|image|picture|long> "
                   "<spans> <json>\n",
                   stderr);
        return 2;
    }
    const int result = paragraph_stages_probe(
        static_cast<std::size_t>(std::distance(paragraph_kinds.begin(), kind)), spans.value(),
        arguments[4]);
    CoUninitialize();
    return result;
}

[[nodiscard]] int probe(std::size_t bytes, std::size_t anchor, const std::string &output)
{
    const std::string timing_path = output + ".timing.json";
    const auto wide = core::to_utf16(timing_path);
    const std::string text = markdown_text(bytes);
    const auto outcome = wide ? timed_run(text, anchor, wide.value()) : std::nullopt;
    const auto timing = read_text(timing_path);
    const auto times = timing ? mark_times(timing.value()) : std::vector<std::int64_t>{};
    if (!outcome || times.size() != timing_marks)
    {
        std::fputs("preview probe: parsing, measuring or the timing file failed\n", stderr);
        return 1;
    }
    std::ofstream file(output, std::ios::binary);
    file << report_of(text, anchors[anchor], outcome.value(), times);
    if (!file)
    {
        std::fputs("preview probe: the output could not be written\n", stderr);
        return 1;
    }
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    const auto arguments = std::span(argv, static_cast<std::size_t>(argc));
    if (arguments.size() == 5 && std::string_view(arguments[1]) == "--long-paragraph")
    {
        return long_paragraph_main(arguments);
    }
    if (arguments.size() == 5 && std::string_view(arguments[1]) == "--paragraph-stages")
    {
        return paragraph_stages_main(arguments);
    }
    const auto bytes = arguments.size() == 4 ? size_of(arguments[1]) : std::nullopt;
    const auto anchor = arguments.size() == 4 ? anchor_of(arguments[2]) : std::nullopt;
    if (!bytes || !anchor)
    {
        std::fputs("usage: nib_preview_probe <bytes> <head|middle|tail> <json>\n", stderr);
        return 2;
    }
    return probe(bytes.value(), anchor.value(), arguments[3]);
}
