// プレビューの縦の送りを要素の中まで画素で畳む（ADR 0155 の決定 7 の 2026-11-20 の追補・#1894）。
// 純関数 preview_scroll_fold（下へ・上へ・端）、PreviewPane の 1 刻みごとの位置（400 行の囲み・
// 長い 1 行の段落で、刻みごとに行の送り × 3 だけ単調に進み、端で止まる）と、当たり判定が描く所と
// 同じずらしを読むこと。
#include "DirectWriteFactory.hpp"
#include "WindowChecks.hpp"

#include "LayoutRect.hpp"
#include "MarkdownDocument.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "Offset.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewCodeColors.hpp"
#include "PreviewFrame.hpp"
#include "PreviewMetrics.hpp"
#include "PreviewPane.hpp"
#include "PreviewPoint.hpp"
#include "PreviewScrollFold.hpp"
#include "PreviewScrollSettled.hpp"
#include "PreviewScrollSpot.hpp"
#include "PreviewSelectionText.hpp"
#include "PreviewTextLayouts.hpp"
#include "PreviewTextStyle.hpp"
#include "PreviewViewport.hpp"
#include "RenderFailure.hpp"
#include "ThemeCatalog.hpp"
#include "ThemeName.hpp"

#include <array>
#include <cmath>
#include <combaseapi.h>
#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace win32 = nenenib::ui::win32;
using win32::PreviewScrollSpot;

constexpr core::LayoutRect area{0, 0, 400, 200};

[[nodiscard]] win32::PreviewTextStyle style_of()
{
    return win32::PreviewTextStyle{"Segoe UI", 13.5F, 96};
}

// 要素の長さの列から引く長さ（測れない番号は失敗）。
[[nodiscard]] win32::PreviewSpanOf spans_of(std::vector<float> spans)
{
    return
        [spans = std::move(spans)](std::size_t index) -> std::expected<float, win32::RenderFailure>
    {
        if (index >= spans.size())
        {
            return std::unexpected(win32::RenderFailure::directwrite);
        }
        return spans.at(index);
    };
}

[[nodiscard]] PreviewScrollSpot folded(PreviewScrollSpot from, const std::vector<float> &spans,
                                       float height)
{
    return win32::preview_scroll_fold(from, spans.size(), height, spans_of(spans))
        .value_or(PreviewScrollSpot{99, -1.0F});
}

// 1. 純関数: 要素の中は nudge だけ、境を越えると次の要素へ、上へは前の要素を足す。端で止まる。
void check_fold()
{
    const std::vector<float> spans{10.0F, 100.0F, 50.0F, 30.0F};
    expect(folded({0, 5.0F}, spans, 40.0F) == PreviewScrollSpot{0, 5.0F} &&
               folded({0, 15.0F}, spans, 40.0F) == PreviewScrollSpot{1, 5.0F} &&
               folded({1, 99.0F}, spans, 40.0F) == PreviewScrollSpot{1, 99.0F},
           "inside an element the nudge moves and crossing its end goes to the next one");
    expect(folded({0, 1000.0F}, spans, 40.0F) == PreviewScrollSpot{3, 0.0F},
           "a short last element stops with its top at the top of the frame");
    expect(folded({0, 1000.0F}, std::vector<float>{10.0F, 100.0F}, 40.0F) ==
               PreviewScrollSpot{1, 60.0F},
           "a tall last element stops with its bottom at the bottom of the frame");
    expect(folded({2, -5.0F}, spans, 40.0F) == PreviewScrollSpot{1, 95.0F} &&
               folded({2, -200.0F}, spans, 40.0F) == PreviewScrollSpot{0, 0.0F},
           "going up adds the previous elements and stops at the top");
    expect(folded({7, 0.0F}, spans, 40.0F) == PreviewScrollSpot{3, 0.0F},
           "an anchor past the document is pulled back to the last element");
    expect(!win32::preview_scroll_fold({0, 15.0F}, 2, 40.0F, spans_of({10.0F})).has_value(),
           "a span that cannot be measured is a failure");
}

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
        FAILED(imaging->CreateBitmap(400, 200, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad,
                                     &bitmap)) ||
        FAILED(factory->CreateWicBitmapRenderTarget(bitmap.Get(), properties, &target)))
    {
        return nullptr;
    }
    return target;
}

[[nodiscard]] std::shared_ptr<const core::MarkdownDocument> parsed(const std::string &text)
{
    auto document = core::parse_markdown(text, core::MarkdownStop::never());
    return document ? std::make_shared<const core::MarkdownDocument>(std::move(document).value())
                    : nullptr;
}

[[nodiscard]] std::string long_fence()
{
    std::string text = "# h\n\n```\n";
    for (int line = 1; line <= 400; ++line)
    {
        text += "code " + std::to_string(line) + "\n";
    }
    return text + "```\n\nafter\n";
}

// 長い 1 行の段落を count 個（日本語の文書に多い形）。
[[nodiscard]] std::string long_paragraphs(std::size_t count)
{
    std::string text;
    for (std::size_t paragraph = 0; paragraph < count; ++paragraph)
    {
        for (int word = 0; word < 120; ++word)
        {
            text += "word" + std::to_string(word) + " ";
        }
        text += "\n\n";
    }
    return text;
}

// 刻みごとの位置（文書の最初からの物理画素）と、枠の上端の囲みのコードの行（文書の通しの番号・
// 囲みの外は 0）。
using Walked = std::vector<std::pair<float, std::size_t>>;

// 文書の最初からの位置（物理画素）。block 0 は置き始める位置が 0、ほかは文書の全体を測った上端。
[[nodiscard]] float absolute_of(const win32::PreviewBlockTops &whole, std::size_t block,
                                float nudge)
{
    return (block == 0 ? 0.0F : whole.top(block)) + win32::preview_pixels(nudge, 96);
}

// 答えの囲みのコードの行の通しの番号（切れは 128 行ずつ・最初の切れは要素 1）。
[[nodiscard]] std::size_t code_line_of(const core::MarkdownDocument &document,
                                       const application::PreviewScrollSettled &settled)
{
    const auto block = core::preview_anchor_block(document, settled.anchor);
    return document.blocks.at(block).kind == core::PreviewBlockKind::code
               ? ((block - 1U) * core::preview_code_piece_lines) + settled.lines + 1U
               : 0U;
}

// steps 回、送り sent の frame を描いて答えを写し（application の写し）、刻みごとの位置を返す。
// 描けなければ空。
[[nodiscard]] Walked walk(win32::PreviewPane &pane, ID2D1RenderTarget &target,
                          const std::shared_ptr<const core::MarkdownDocument> &document,
                          std::pair<int, std::int64_t> steps)
{
    win32::PreviewTextLayouts layouts;
    const auto whole = win32::PreviewBlockTops::measure(
        *document, layouts, style_of(),
        win32::PreviewViewport{0, 400.0F, std::numeric_limits<float>::max()});
    Walked positions;
    core::Offset anchor{0};
    float nudge = 0.0F;
    std::size_t line = 0;
    for (int step = 0; step < steps.first && whole; ++step)
    {
        target.BeginDraw();
        const auto painted = pane.paint(
            target,
            application::PreviewFrame{
                document, anchor, {}, {}, nullptr, {}, std::nullopt, nudge, steps.second},
            style_of(), area);
        if (FAILED(target.EndDraw()) || !painted)
        {
            return {};
        }
        if (const auto &settled = pane.settled(); settled.has_value())
        {
            anchor = settled.value().anchor;
            nudge = settled.value().nudge;
            line = code_line_of(*document, settled.value());
        }
        positions.emplace_back(
            absolute_of(whole.value(), core::preview_anchor_block(*document, anchor), nudge), line);
    }
    return positions;
}

// 位置が刻みごとに advance ずつ進み、最後に端で止まったか（止まる刻みは advance 未満だけ進む）。
[[nodiscard]] bool steady(const Walked &positions, float advance)
{
    const auto moved = [&positions](std::size_t at)
    { return positions.at(at).first - positions.at(at - 1U).first; };
    std::size_t at = 1;
    while (at < positions.size() && std::fabs(moved(at) - advance) <= 0.01F)
    {
        ++at;
    }
    if (at == positions.size() || moved(at) < -0.01F || moved(at) > advance)
    {
        return false;
    }
    for (++at; at < positions.size(); ++at)
    {
        if (std::fabs(moved(at)) > 0.01F)
        {
            return false;
        }
    }
    return true;
}

// 囲みの中の刻みで、枠の上端の行が 3 行ずつ進んだ刻みの数と、囲みの中の刻みの数。
[[nodiscard]] std::pair<std::size_t, std::size_t> code_steps(const Walked &positions)
{
    std::size_t three = 0;
    std::size_t inside = 0;
    for (std::size_t at = 1; at < positions.size(); ++at)
    {
        const auto before = positions.at(at - 1U).second;
        const auto after = positions.at(at).second;
        if (before > 0 && after > 0)
        {
            ++inside;
            three += after == before + 3U ? 1U : 0U;
        }
    }
    return {three, inside};
}

// 2. 400 行の囲みと長い 1 行の段落: 1 刻みごとに行の送り × 3 だけ進み、端で止まる。戻りも同じ。
void check_walks()
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    win32::PreviewPane pane;
    if (target == nullptr || !theme || !pane.prepare(*target.Get(), theme.value().view()))
    {
        expect(false, "the preview pane prepares for the walks");
        return;
    }
    const auto advance = 3.0F * win32::preview_line_pixels(style_of());
    const auto fence = parsed(long_fence());
    const auto paragraphs = parsed(long_paragraphs(40));
    if (fence == nullptr || paragraphs == nullptr)
    {
        expect(false, "the walk documents parse");
        return;
    }
    const auto down = walk(pane, *target.Get(), fence, {200, 3});
    const auto [three, inside] = code_steps(down);
    expect(down.size() == 200U && steady(down, advance),
           "every wheel step moves the fence by three lines until the end");
    expect(inside > 120U && three == inside,
           "the line at the top of the frame moves three lines a step through every piece");
    const auto prose = walk(pane, *target.Get(), paragraphs, {400, 3});
    expect(prose.size() == 400U && steady(prose, advance),
           "every wheel step moves long one-line paragraphs by three lines until the end");
}

constexpr std::int32_t shift = 12;
using Points = std::array<std::optional<core::PreviewPoint>, 3>;

// 枠の上端から top の下に 40 画素おきの 3 点の文字の位置。
[[nodiscard]] Points points_below(win32::PreviewPane &pane, std::int32_t top)
{
    Points points{};
    for (std::size_t row = 0; row < points.size(); ++row)
    {
        points.at(row) = pane.point_at(40, top + 30 + (static_cast<std::int32_t>(row) * 40));
    }
    return points;
}

// 2 つ目のタスクのチェックボックスに当たる最初の点（枠の上端から shift より下）。
[[nodiscard]] std::optional<std::pair<std::int32_t, std::int32_t>>
second_task_point(const win32::PreviewPane &pane)
{
    for (std::int32_t at = 0; at < 80 * 120; ++at)
    {
        const auto x = at % 80;
        const auto y = shift + (at / 80);
        if (pane.task_at(x, y) == std::optional<std::size_t>{1})
        {
            return std::pair{x, y};
        }
    }
    return std::nullopt;
}

// 3. 送った枠の当たり判定は描く所と同じずらしを読む（要素の上端を -nudge に置く）。
void check_hits_follow_nudge()
{
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    const auto target = wic_target(bitmap);
    const auto catalog = core::ThemeCatalog::builtins();
    const auto theme = catalog.find(core::ThemeName::parse("ubuntu-aubergine").value());
    const auto document = parsed("- [ ] task one\n- [ ] task two\n\n" + long_paragraphs(3));
    win32::PreviewPane pane;
    if (target == nullptr || !theme || document == nullptr ||
        !pane.prepare(*target.Get(), theme.value().view()))
    {
        expect(false, "the preview pane prepares for the hits");
        return;
    }
    const auto paint_at = [&](float nudge)
    {
        target->BeginDraw();
        const auto painted =
            pane.paint(*target.Get(),
                       application::PreviewFrame{
                           document, core::Offset{0}, {}, {}, nullptr, {}, std::nullopt, nudge, 0},
                       style_of(), area);
        return SUCCEEDED(target->EndDraw()) && painted.has_value();
    };
    const bool first = paint_at(0.0F);
    const auto still = points_below(pane, shift);
    const auto task_point = second_task_point(pane);
    const auto box_still = pane.range_box(2, 0, 4);
    const bool second = paint_at(static_cast<float>(shift));
    const auto moved = points_below(pane, 0);
    const auto box_moved = pane.range_box(2, 0, 4);
    expect(first && second && still.at(0).has_value() && still == moved,
           "a point under the pointer finds the same character after the nudge");
    expect(task_point.has_value() &&
               pane.task_at(task_point.value().first, task_point.value().second - shift) ==
                   std::optional<std::size_t>{1},
           "a checkbox is pressed where it is drawn after the nudge");
    expect(box_still.has_value() && box_moved.has_value() &&
               box_moved.value().top == box_still.value().top - shift,
           "the box of a range moves up by the nudge");
    expect(!pane.settled().has_value(), "a settled position without pending sends no answer");
}
} // namespace

void verify_preview_scroll_fold()
{
    const auto factory = directwrite_factory();
    check_fold();
    if (!factory || !installed(*factory.Get(), L"Segoe UI"))
    {
        unmeasured("#1894 preview scroll walks need Segoe UI");
        return;
    }
    check_walks();
    check_hits_follow_nudge();
}
} // namespace nenenib::tests::ui
