// 字形の保持の collector（ADR 0073）を確かめる。
// 未対応の装飾は収集の全体を失敗させ、0 字形の run は描画へ渡さず、
// 完全に画面外の run だけを境界つきで除く（横と、#528 の縦）。画面外の判定は本物の face の寸法を
// 読むので、フォントが 1 つも無い機械では測れない（環境依存・QLT-013）。
#include "BodyGlyphCollector.hpp"
#include "DevicePixels.hpp"
#include "DirectWriteFactory.hpp"
#include "WindowChecks.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <dwrite_1.h>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Collector = Microsoft::WRL::ComPtr<win32::BodyGlyphCollector>;

constexpr FLOAT screen_width = 100;
constexpr FLOAT run_em = 20;
constexpr std::array<UINT16, 2> run_indices{0, 0};
constexpr std::array<FLOAT, 2> run_advances{10, -4};
constexpr std::array<DWRITE_GLYPH_OFFSET, 2> run_offsets{DWRITE_GLYPH_OFFSET{5, 0},
                                                         DWRITE_GLYPH_OFFSET{-7, 0}};

// 描画の前提（ADR 0073・draw_body_text のコメント）: DPI 96・恒等・画素の位置を丸める。
void verify_drawing_assumptions()
{
    const Collector collector = Microsoft::WRL::Make<win32::BodyGlyphCollector>(screen_width);
    FLOAT pixels = 0;
    DWRITE_MATRIX transform{};
    BOOL disabled = TRUE;
    expect(core::reference_dpi == 96, "the renderer draws at 96 DPI");
    expect(SUCCEEDED(collector->GetPixelsPerDip(nullptr, &pixels)) && pixels == 1,
           "the collector reports one pixel per DIP");
    expect(SUCCEEDED(collector->GetCurrentTransform(nullptr, &transform)) && transform.m11 == 1 &&
               transform.m12 == 0 && transform.m21 == 0 && transform.m22 == 1 &&
               transform.dx == 0 && transform.dy == 0,
           "the collector reports the identity transform");
    expect(SUCCEEDED(collector->IsPixelSnappingDisabled(nullptr, &disabled)) && disabled == FALSE,
           "the collector keeps pixel snapping");
}

// 1 つの callback を受けた新しい collector が字形を渡すか。
bool gives_glyphs(const Collector &collector)
{
    return collector->take().has_value();
}

Collector fresh()
{
    return Microsoft::WRL::Make<win32::BodyGlyphCollector>(screen_width);
}

void verify_unsupported_callbacks()
{
    const DWRITE_GLYPH_RUN empty{nullptr, run_em, 0, nullptr, nullptr, nullptr, FALSE, 0};
    Collector collector = fresh();
    expect(collector->DrawUnderline(nullptr, 0, 0, nullptr, nullptr) == E_NOTIMPL &&
               !gives_glyphs(collector),
           "an underline rejects the collection");
    collector = fresh();
    expect(collector->DrawStrikethrough(nullptr, 0, 0, nullptr, nullptr) == E_NOTIMPL &&
               !gives_glyphs(collector),
           "a strikethrough rejects the collection");
    collector = fresh();
    expect(collector->DrawInlineObject(nullptr, 0, 0, nullptr, FALSE, FALSE, nullptr) ==
                   E_NOTIMPL &&
               !gives_glyphs(collector),
           "an inline object rejects the collection");
    collector = fresh();
    expect(collector->DrawGlyphRun(nullptr, 0, 0, DWRITE_MEASURING_MODE_NATURAL, &empty, nullptr,
                                   collector.Get()) == E_NOTIMPL &&
               !gives_glyphs(collector),
           "a run with a drawing effect rejects the collection");
    collector = fresh();
    expect(collector->DrawGlyphRun(nullptr, 0, 0, DWRITE_MEASURING_MODE_NATURAL, &empty, nullptr,
                                   nullptr) == S_OK,
           "a run of zero glyphs (Tab) is accepted");
    const auto taken = collector->take();
    expect(taken.has_value() && taken.value().empty(),
           "a run of zero glyphs is not passed to drawing and does not reject");
}

using Layout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;
using Glyphs = std::optional<std::vector<win32::BodyGlyphRun>>;

Layout layout_of(IDWriteFactory2 &factory, IDWriteTextFormat *format, const std::wstring &text)
{
    Layout layout;
    if (FAILED(factory.CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format,
                                        screen_width, run_em * 2, layout.GetAddressOf())))
    {
        return nullptr;
    }
    return layout;
}

// Direct2DRenderer::layout_of と同じく Draw の後に take() で決める。
// Draw の HRESULT は callback の失敗を返さない（2026-10-07 実測）ので見ない。
Glyphs collected(const Layout &layout)
{
    if (layout == nullptr)
    {
        return std::nullopt;
    }
    const Collector collector = fresh();
    static_cast<void>(layout->Draw(nullptr, collector.Get(), 0, 0));
    return collector->take();
}

bool rejected(const Layout &layout)
{
    return layout != nullptr && !collected(layout).has_value();
}

void verify_layout_decorations(IDWriteFactory2 &factory, IDWriteTextFormat *format)
{
    const DWRITE_TEXT_RANGE all{0, 5};
    const Glyphs plain = collected(layout_of(factory, format, L"ab\tcd"));
    expect(plain.has_value() && !plain.value().empty() &&
               std::ranges::none_of(plain.value(), [](const win32::BodyGlyphRun &run)
                                    { return run.indices.empty(); }),
           "a plain line with a Tab gives glyphs and no run of zero glyphs");
    const Layout underlined = layout_of(factory, format, L"ab\tcd");
    expect(underlined != nullptr && SUCCEEDED(underlined->SetUnderline(TRUE, all)) &&
               rejected(underlined),
           "a layout with an underline gives no glyphs at all");
    const Layout struck = layout_of(factory, format, L"ab\tcd");
    expect(struck != nullptr && SUCCEEDED(struck->SetStrikethrough(TRUE, all)) && rejected(struck),
           "a layout with a strikethrough gives no glyphs at all");
    Microsoft::WRL::ComPtr<IDWriteInlineObject> sign;
    expect(SUCCEEDED(factory.CreateEllipsisTrimmingSign(format, sign.GetAddressOf())),
           "an inline object is made");
    const Layout inlined = layout_of(factory, format, L"ab\tcd");
    expect(inlined != nullptr &&
               SUCCEEDED(inlined->SetInlineObject(sign.Get(), DWRITE_TEXT_RANGE{0, 1})) &&
               rejected(inlined),
           "a layout with an inline object gives no glyphs at all");
    const Layout effected = layout_of(factory, format, L"ab\tcd");
    expect(effected != nullptr && SUCCEEDED(effected->SetDrawingEffect(sign.Get(), all)) &&
               rejected(effected),
           "a layout with a drawing effect gives no glyphs at all");
}

// BodyGlyphCollector::outside と同じ順で同じ式を足す（float の丸めも同じになる）。
FLOAT bound_of(const DWRITE_FONT_METRICS1 &metrics, bool with_offsets)
{
    const float extent = std::max(std::abs(static_cast<float>(metrics.glyphBoxLeft)),
                                  std::abs(static_cast<float>(metrics.glyphBoxRight)));
    float bound = extent * run_em / static_cast<float>(metrics.designUnitsPerEm) + 2;
    for (const auto advance : run_advances)
    {
        bound += std::abs(advance);
    }
    return with_offsets ? bound + 7 : bound;
}

bool kept(IDWriteFontFace *face, FLOAT x, BOOL sideways, const DWRITE_GLYPH_OFFSET *offsets)
{
    const Collector collector = Microsoft::WRL::Make<win32::BodyGlyphCollector>(screen_width);
    const DWRITE_GLYPH_RUN run{face,
                               run_em,
                               static_cast<UINT32>(run_indices.size()),
                               run_indices.data(),
                               run_advances.data(),
                               offsets,
                               sideways,
                               0};
    expect(collector->DrawGlyphRun(nullptr, x, 0, DWRITE_MEASURING_MODE_NATURAL, &run, nullptr,
                                   nullptr) == S_OK,
           "a visible or hidden run is not an error");
    const auto taken = collector->take();
    if (!taken.has_value())
    {
        return false;
    }
    const auto &runs = taken.value();
    return runs.size() == 1 && runs.front().origin.x == x && runs.front().advances.size() == 2 &&
           runs.front().offsets.size() == (offsets == nullptr ? 0U : 2U);
}

void verify_outside_bounds(IDWriteFontFace1 &face)
{
    DWRITE_FONT_METRICS1 metrics{};
    face.GetMetrics(&metrics);
    const FLOAT bound = bound_of(metrics, false);
    const FLOAT below = -std::numeric_limits<FLOAT>::infinity();
    expect(kept(&face, -bound, FALSE, nullptr), "a run touching the left edge is kept");
    expect(!kept(&face, std::nextafter(-bound, below), FALSE, nullptr),
           "a run just past the left edge is dropped");
    expect(kept(&face, screen_width + bound - 1, FALSE, nullptr),
           "a run inside the right bound is kept");
    expect(!kept(&face, screen_width + bound + 1, FALSE, nullptr),
           "a run past the right bound is dropped");
    expect(kept(&face, -bound - 3, FALSE, run_offsets.data()),
           "glyph offsets widen the bound by the largest offset");
    expect(!kept(&face, -bound_of(metrics, true) - 1, FALSE, run_offsets.data()),
           "a run past the widened bound is dropped");
    expect(kept(&face, -bound * 4, TRUE, nullptr), "a sideways run is never dropped");
}

// BodyGlyphCollector の縦の境界と同じ順で同じ式を足す（glyphOffsets なし）。
std::pair<FLOAT, FLOAT> vertical_of(const DWRITE_FONT_METRICS1 &metrics)
{
    const auto ascent = static_cast<float>(metrics.ascent);
    const auto descent = static_cast<float>(metrics.descent);
    const float margin = ascent + descent + std::abs(static_cast<float>(metrics.lineGap));
    const float above = std::max(ascent, static_cast<float>(metrics.glyphBoxTop)) + margin;
    const float below = std::max(descent, -static_cast<float>(metrics.glyphBoxBottom)) + margin;
    const auto units = static_cast<float>(metrics.designUnitsPerEm);
    return {above * run_em / units + 2, below * run_em / units + 2};
}

constexpr FLOAT drawn_top = 100;
constexpr FLOAT drawn_bottom = 160;

bool kept_at(IDWriteFontFace *face, FLOAT y, bool bounded = true)
{
    const Collector collector =
        bounded ? Microsoft::WRL::Make<win32::BodyGlyphCollector>(screen_width, 0, drawn_top,
                                                                  drawn_bottom)
                : Microsoft::WRL::Make<win32::BodyGlyphCollector>(screen_width);
    const DWRITE_GLYPH_RUN run{face,
                               run_em,
                               static_cast<UINT32>(run_indices.size()),
                               run_indices.data(),
                               run_advances.data(),
                               nullptr,
                               FALSE,
                               0};
    expect(collector->DrawGlyphRun(nullptr, 10, y, DWRITE_MEASURING_MODE_NATURAL, &run, nullptr,
                                   nullptr) == S_OK,
           "a run above or below the drawn rows is not an error");
    const auto taken = collector->take();
    return taken.has_value() && taken.value().size() == 1 && taken.value().front().origin.y == y;
}

// #528: 縦に描く範囲と交わらない run だけを除く。字の箱が半分かかる run は残す（保守的）。
void verify_vertical_bounds(IDWriteFontFace1 &face)
{
    DWRITE_FONT_METRICS1 metrics{};
    face.GetMetrics(&metrics);
    const auto [above, below] = vertical_of(metrics);
    const auto units = static_cast<float>(metrics.designUnitsPerEm);
    const float ascent = static_cast<float>(metrics.ascent) * run_em / units;
    const float descent = static_cast<float>(metrics.descent) * run_em / units;
    const FLOAT up = -std::numeric_limits<FLOAT>::infinity();
    const FLOAT down = std::numeric_limits<FLOAT>::infinity();
    expect(kept_at(&face, drawn_top + (ascent - descent) / 2),
           "a run whose box is half above the drawn rows is kept");
    expect(kept_at(&face, drawn_bottom + (ascent - descent) / 2),
           "a run whose box is half below the drawn rows is kept");
    expect(kept_at(&face, drawn_top - below), "a run reaching the top edge is kept");
    expect(!kept_at(&face, std::nextafter(drawn_top - below, up)),
           "a run just above the top bound is dropped");
    expect(kept_at(&face, drawn_bottom + above), "a run reaching the bottom edge is kept");
    expect(!kept_at(&face, std::nextafter(drawn_bottom + above, down)),
           "a run just below the bottom bound is dropped");
    expect(kept_at(&face, drawn_bottom * 1000, false) &&
               kept_at(&face, -drawn_bottom * 1000, false),
           "a collector without drawn rows never drops a run vertically");
}

// 折り返した layout の各行の top と baseline（layout の座標）。
std::vector<std::pair<FLOAT, FLOAT>> rows_of(const Layout &layout)
{
    UINT32 count = 0;
    static_cast<void>(layout->GetLineMetrics(nullptr, 0, &count));
    std::vector<DWRITE_LINE_METRICS> lines(count);
    if (FAILED(layout->GetLineMetrics(lines.data(), count, &count)))
    {
        return {};
    }
    std::vector<std::pair<FLOAT, FLOAT>> rows;
    FLOAT top = 0;
    for (const auto &line : lines)
    {
        rows.emplace_back(top, top + line.baseline);
        top += line.height;
    }
    return rows;
}

std::set<FLOAT> collected_baselines(const Layout &layout, FLOAT top, FLOAT bottom)
{
    const Collector collector =
        Microsoft::WRL::Make<win32::BodyGlyphCollector>(screen_width, 0, top, bottom);
    static_cast<void>(layout->Draw(nullptr, collector.Get(), 0, 0));
    std::set<FLOAT> baselines;
    for (const auto &run : collector->take().value_or(std::vector<win32::BodyGlyphRun>{}))
    {
        baselines.insert(run.origin.y);
    }
    return baselines;
}

bool has_baseline(const std::set<FLOAT> &baselines, FLOAT expected)
{
    return std::ranges::any_of(baselines, [expected](FLOAT value)
                               { return std::abs(value - expected) < 0.5F; });
}

// #528: wrap on で行高の 100 倍を超える layout のうち、見えている 3 行の近くの run だけが集まる。
void verify_tall_layout(IDWriteFactory2 &factory, IDWriteTextFormat *format)
{
    std::wstring text;
    for (int word = 0; word < 160; ++word)
    {
        text += L"abcdefg ";
    }
    const Layout layout = layout_of(factory, format, text);
    const auto rows = layout == nullptr ? std::vector<std::pair<FLOAT, FLOAT>>{} : rows_of(layout);
    expect(rows.size() >= 100, "the wrapped layout is at least 100 lines tall");
    if (rows.size() < 100)
    {
        return;
    }
    const auto baselines = collected_baselines(layout, rows.at(50).first, rows.at(53).first);
    expect(has_baseline(baselines, rows.at(50).second) &&
               has_baseline(baselines, rows.at(51).second) &&
               has_baseline(baselines, rows.at(52).second),
           "the three drawn rows are collected");
    expect(baselines.size() < 12 && !baselines.empty() && *baselines.begin() > rows.at(44).second &&
               *baselines.rbegin() < rows.at(59).second,
           "rows far above and below the drawn rows are not collected");
}

Microsoft::WRL::ComPtr<IDWriteFontFace1> first_face(IDWriteFactory2 &factory)
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
    Microsoft::WRL::ComPtr<IDWriteFont> font;
    Microsoft::WRL::ComPtr<IDWriteFontFace> face;
    Microsoft::WRL::ComPtr<IDWriteFontFace1> face1;
    if (FAILED(factory.GetSystemFontCollection(collection.GetAddressOf(), FALSE)) ||
        collection->GetFontFamilyCount() == 0 ||
        FAILED(collection->GetFontFamily(0, family.GetAddressOf())) ||
        FAILED(family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                            DWRITE_FONT_STYLE_NORMAL, font.GetAddressOf())) ||
        FAILED(font->CreateFontFace(face.GetAddressOf())) || FAILED(face.As(&face1)))
    {
        return nullptr;
    }
    return face1;
}
} // namespace

void verify_glyph_collector()
{
    verify_drawing_assumptions();
    verify_unsupported_callbacks();
    const auto factory = directwrite_factory();
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    expect(factory != nullptr &&
               SUCCEEDED(factory->CreateTextFormat(
                   L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                   DWRITE_FONT_STRETCH_NORMAL, run_em, L"", format.GetAddressOf())),
           "a DirectWrite text format is made");
    if (factory == nullptr || format == nullptr)
    {
        return;
    }
    verify_layout_decorations(*factory.Get(), format.Get());
    verify_tall_layout(*factory.Get(), format.Get());
    const auto face = first_face(*factory.Get());
    if (face == nullptr)
    {
        unmeasured("the outside bound needs an installed font face");
        return;
    }
    verify_outside_bounds(*face.Get());
    verify_vertical_bounds(*face.Get());
}
} // namespace nenenib::tests::ui
