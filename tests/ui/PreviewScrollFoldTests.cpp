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
#include "PreviewAnchoredBlock.hpp"
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
#include <tuple>
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
// 囲みの外は 0）。先頭は歩き始める前の位置。
using Walked = std::vector<std::pair<float, std::size_t>>;
// 刻みの間で application が持つ位置の写し（錨・要素の番号・nudge）。
using Place = std::tuple<core::Offset, std::optional<std::size_t>, float>;
// 描く枠と描く先。
using Painter = std::pair<win32::PreviewPane *, ID2D1RenderTarget *>;

// 文書の最初からの位置（物理画素）。block 0 は置き始める位置が 0、ほかは文書の全体を測った上端。
[[nodiscard]] float absolute_of(const win32::PreviewBlockTops &whole, std::size_t block,
                                float nudge)
{
    return (block == 0 ? 0.0F : whole.top(block)) + win32::preview_pixels(nudge, 96);
}

// 答えの囲みのコードの行の通しの番号（切れは preview_code_piece_lines 行ずつ・最初の切れは要素
// 1）。
[[nodiscard]] std::size_t code_line_of(const core::MarkdownDocument &document,
                                       const application::PreviewScrollSettled &settled)
{
    return document.blocks.at(settled.block).kind == core::PreviewBlockKind::code
               ? ((settled.block - 1U) * core::preview_code_piece_lines) + settled.lines + 1U
               : 0U;
}

// place から steps.first 回、送り steps.second の frame を描いて答えを写し（application の写し・
// 要素の番号も運ぶ）、刻みごとの位置を返す。place は歩いた後の位置になる。描けなければ空。
[[nodiscard]] Walked walk(Painter painter,
                          const std::shared_ptr<const core::MarkdownDocument> &document,
                          std::pair<int, std::int64_t> steps, Place &place)
{
    win32::PreviewTextLayouts layouts;
    const auto whole = win32::PreviewBlockTops::measure(
        *document, layouts, style_of(),
        win32::PreviewViewport{0, 400.0F, std::numeric_limits<float>::max()});
    if (!whole)
    {
        return {};
    }
    auto &[anchor, block, nudge] = place;
    const auto here = [&]
    {
        return absolute_of(whole.value(),
                           application::preview_anchored_block(*document, anchor, block), nudge);
    };
    Walked positions{{here(), 0U}};
    auto &[pane, target] = painter;
    for (int step = 0; step < steps.first; ++step)
    {
        target->BeginDraw();
        const auto painted = pane->paint(
            *target,
            application::PreviewFrame{
                document, anchor, {}, {}, nullptr, {}, std::nullopt, nudge, steps.second, block},
            style_of(), area);
        if (FAILED(target->EndDraw()) || !painted)
        {
            return {};
        }
        std::size_t line = positions.back().second;
        if (const auto &settled = pane->settled(); settled.has_value())
        {
            anchor = settled.value().anchor;
            block = settled.value().block;
            nudge = settled.value().nudge;
            line = code_line_of(*document, settled.value());
        }
        positions.emplace_back(here(), line);
    }
    return positions;
}

// 位置が刻みごとに advance（負なら上へ）ずつ動き、最後に端で止まったか（止まる刻みは advance
// より少なく同じ向きに動く）。
[[nodiscard]] bool steady(const Walked &positions, float advance)
{
    const auto ratio = [&positions, advance](std::size_t at)
    { return (positions.at(at).first - positions.at(at - 1U).first) / advance; };
    std::size_t at = 1;
    while (at < positions.size() && std::fabs(ratio(at) - 1.0F) <= 0.001F)
    {
        ++at;
    }
    if (at == positions.size() || ratio(at) < -0.001F || ratio(at) > 1.0F)
    {
        return false;
    }
    for (++at; at < positions.size(); ++at)
    {
        if (std::fabs(ratio(at)) > 0.001F)
        {
            return false;
        }
    }
    return true;
}

// 囲みの中の刻みで、枠の上端の行が lines 行ずつ動いた刻みの数と、囲みの中の刻みの数。
[[nodiscard]] std::pair<std::size_t, std::size_t> code_steps(const Walked &positions,
                                                             std::int64_t lines)
{
    std::size_t matched = 0;
    std::size_t inside = 0;
    for (std::size_t at = 2; at < positions.size(); ++at)
    {
        const auto before = static_cast<std::int64_t>(positions.at(at - 1U).second);
        const auto after = static_cast<std::int64_t>(positions.at(at).second);
        if (before > 0 && after > 0)
        {
            ++inside;
            matched += after == before + lines ? 1U : 0U;
        }
    }
    return {matched, inside};
}

// 下りで端まで歩き、そこから上りで先頭まで戻る。どちらも刻みごとに同じ量だけ動いて端で止まり、
// 端で止まった後の逆向きの最初の 1 刻みはすぐ動くか。
[[nodiscard]] bool round_trip(Painter painter,
                              const std::shared_ptr<const core::MarkdownDocument> &document,
                              int steps, float advance)
{
    Place place{core::Offset{0}, std::nullopt, 0.0F};
    const auto down = walk(painter, document, {steps, 3}, place);
    const auto up = walk(painter, document, {steps, -3}, place);
    const auto back = walk(painter, document, {1, 1}, place);
    const auto fine = walk(painter, document, {steps * 3, -1}, place);
    return down.size() == static_cast<std::size_t>(steps) + 1U && steady(down, advance) &&
           up.size() == down.size() && steady(up, -advance) &&
           std::fabs(up.at(1).first - up.at(0).first + advance) <= 0.01F && back.size() == 2U &&
           std::fabs(back.at(1).first - back.at(0).first - (advance / 3.0F)) <= 0.01F &&
           steady(fine, -advance / 3.0F);
}

// 2. 400 行の囲み・長い 1 行の段落・空の項目の並び（同じ source_begin の要素・レビュー M1）:
// 1 刻みごとに行の送り × 3 だけ動き、端で止まる。上りも同じで、端の後の逆向きはすぐ動く。
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
    const Painter painter{&pane, target.Get()};
    const auto advance = 3.0F * win32::preview_line_pixels(style_of());
    const auto fence = parsed(long_fence());
    const auto paragraphs = parsed(long_paragraphs(40));
    const auto items =
        parsed(long_paragraphs(2) + "-\n-\n-\n-\n\n***\n\n***\n\n" + long_paragraphs(2));
    if (fence == nullptr || paragraphs == nullptr || items == nullptr)
    {
        expect(false, "the walk documents parse");
        return;
    }
    Place place{core::Offset{0}, std::nullopt, 0.0F};
    const auto down = walk(painter, fence, {200, 3}, place);
    const auto [three, inside] = code_steps(down, 3);
    expect(down.size() == 201U && steady(down, advance),
           "every wheel step moves the fence by three lines until the end");
    expect(inside > 120U && three == inside,
           "the line at the top of the frame moves three lines a step through every piece");
    const auto up = walk(painter, fence, {200, -3}, place);
    const auto [back, inside_up] = code_steps(up, -3);
    expect(steady(up, -advance) && inside_up > 120U && back == inside_up,
           "going up moves the fence back three lines a step through every piece");
    expect(round_trip(painter, paragraphs, 400, advance),
           "long one-line paragraphs move steadily both ways and turn at once at the ends");
    expect(round_trip(painter, items, 120, advance),
           "elements sharing one source position never move the wrong way nor jump");
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
