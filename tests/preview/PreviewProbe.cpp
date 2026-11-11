// Markdown プレビューの差し替えの費用の probe（#818・ADR 0155 の決定 7）。CTest に登録しない。
// 決定的に作った Markdown を parse_markdown で読み、PreviewTextLayouts ＋ PreviewBlockTops::measure
// で錨から 1 枠ぶん測る。製品の PreviewPane は通さない（nib_preview_capture と同じ土台）。
// 時刻は Win32TimingAdapter の節目だけで取る（ARC-007）: parse と measure をそれぞれ
// probe_started / probe_finished の 1 対で挟み、節目の
// JSON（<json>.timing.json）を読み戻して差を書く。
// --long-paragraph（#1477）は行内コードの多い 1 つの長い段落（36 KB 前後・span の数は引数）の
// 文書を WIC の描画先に製品と同じ PreviewPainter で描く。最初の 1 回（layout を作って色を置く）と、
// 錨を 0 と 1 で動かしながらの描き直し repaints 回を別の節目の対で測り、描き直しの間に layout を
// 作った数・SetDrawingEffect・HitTestTextRange の数を書く（保持した layout を汚さないことの数）。
//
//   nib_preview_probe <bytes> <head|middle|tail> <json の出力先>
//   nib_preview_probe --long-paragraph <spans> <repaints> <json の出力先>
#include "PreviewProbeOutcome.hpp"
#include "Sha256.hpp"

#include "Appearance.hpp"
#include "DevicePixels.hpp"
#include "EditorSettings.hpp"
#include "FontSize.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "Milestone.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewBrushes.hpp"
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

// 描く先と色（描き直しの間は同じ）。
using Canvas = std::pair<ID2D1RenderTarget *, const win32::PreviewBrushes *>;

// 錨 first から測って 1 回描く。失敗は false。
[[nodiscard]] bool paint_once(const Canvas &canvas, const core::MarkdownDocument &document,
                              win32::PreviewTextLayouts &layouts, std::size_t first)
{
    auto &target = *canvas.first;
    const win32::PreviewTextStyle style{
        std::string(core::default_editor_settings().font_family.text()),
        core::FontSize::default_points, viewport_dpi};
    const auto width = pixels_of(viewport_width);
    const auto height = pixels_of(viewport_height);
    const auto tops = win32::PreviewBlockTops::measure(
        document, layouts, style, win32::PreviewViewport{first, width, height});
    if (!tops)
    {
        return false;
    }
    std::optional<win32::RenderFailure> failure;
    win32::PreviewPainter<ID2D1RenderTarget> painter(target, layouts, *canvas.second, failure);
    target.BeginDraw();
    painter.paint(win32::PreviewScene{
        document, tops.value(), style,
        core::LayoutRect{0, 0, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height)},
        0.0F, std::nullopt});
    return SUCCEEDED(target.EndDraw()) && !failure.has_value();
}

using LayoutCounts = std::tuple<std::size_t, std::size_t, std::size_t>;

[[nodiscard]] LayoutCounts counts_of(const win32::PreviewTextLayouts &layouts)
{
    return {layouts.created(), layouts.inked(), layouts.hit_tests()};
}

// 最初の 1 回と描き直しを節目の対で挟む。返すのは最初の 1 回の後と描き直しの後の数。
[[nodiscard]] std::optional<std::pair<LayoutCounts, LayoutCounts>>
timed_paints(const core::MarkdownDocument &document, std::size_t repaints,
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
    timing.mark(core::Milestone::probe_started);
    const Canvas canvas{target.Get(), &brushes};
    bool painted = paint_once(canvas, document, layouts, 1);
    timing.mark(core::Milestone::probe_finished);
    const auto first = counts_of(layouts);
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < repaints && painted; ++index)
    {
        painted = paint_once(canvas, document, layouts, index % 2);
    }
    timing.mark(core::Milestone::probe_finished);
    if (!painted)
    {
        return std::nullopt;
    }
    return std::pair{first, counts_of(layouts)};
}

// sizes は span の数と描き直しの回数。
[[nodiscard]] std::string long_report_of(const std::string &text,
                                         std::pair<std::size_t, std::size_t> sizes,
                                         const std::pair<LayoutCounts, LayoutCounts> &counts,
                                         std::span<const std::int64_t> times)
{
    const auto &[spans, repaints] = sizes;
    const auto &[first, last] = counts;
    return std::format(
        "{{\"schema\":1,\"probe\":\"nib_preview_probe\",\"mode\":\"long-paragraph\","
        "\"bytes\":{},\"sha256\":\"{}\",\"spans\":{},\"repaints\":{},\"firstPaintMs\":{},"
        "\"repaintsMs\":{},\"firstLayoutsCreated\":{},\"firstDrawingEffects\":{},"
        "\"firstHitTests\":{},\"repaintLayoutsCreated\":{},\"repaintDrawingEffects\":{},"
        "\"repaintHitTests\":{},\"viewport\":{{\"widthDip\":{},\"heightDip\":{},\"dpi\":{}}},"
        "\"commit\":\"{}\",\"dirty\":{},\"configuration\":\"{}\"}}\n",
        text.size(), nenenib::tests::preview::sha256_hex(text), spans, repaints,
        milliseconds_text(times[1] - times[0]), milliseconds_text(times[3] - times[2]),
        std::get<0>(first), std::get<1>(first), std::get<2>(first),
        std::get<0>(last) - std::get<0>(first), std::get<1>(last) - std::get<1>(first),
        std::get<2>(last) - std::get<2>(first), viewport_width, viewport_height, viewport_dpi,
        NIB_PROBE_COMMIT, NIB_PROBE_DIRTY, NIB_PROBE_CONFIGURATION);
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
    if (!counts || times.size() != timing_marks)
    {
        std::fputs("preview probe: parsing, painting or the timing file failed\n", stderr);
        return 1;
    }
    std::ofstream file(output, std::ios::binary);
    file << long_report_of(text, std::pair{spans, repaints}, counts.value(), times);
    return file ? 0 : 1;
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
        const auto spans = size_of(arguments[2]);
        const auto repaints = size_of(arguments[3]);
        if (!spans || !repaints || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
        {
            std::fputs("usage: nib_preview_probe --long-paragraph <spans> <repaints> <json>\n",
                       stderr);
            return 2;
        }
        const int result = long_paragraph_probe(spans.value(), repaints.value(), arguments[4]);
        CoUninitialize();
        return result;
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
