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
#include <span>
#include <string>
#include <tuple>
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
// run の単位の境界を確かめる run は 1 字形（2 字形以上は描く窓を字形の単位で狭める・#586 の工程
// 2）。
constexpr std::array<UINT16, 1> run_indices{0};
constexpr std::array<FLOAT, 1> run_advances{10};
constexpr std::array<DWRITE_GLYPH_OFFSET, 1> run_offsets{DWRITE_GLYPH_OFFSET{-7, 0}};

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
    return runs.size() == 1 && runs.front().origin.x == x &&
           runs.front().advances.size() == run_advances.size() &&
           runs.front().offsets.size() == (offsets == nullptr ? 0U : run_offsets.size());
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

// #586 の工程 2: 横に画面と交わる長い run
// は、字形の配列を全体のまま持ち、描く窓を画面の近くに狭める。 1,000 字形・送り 10 のうち画面（幅
// 400）に掛かるのは 300〜339 の 40 字形。
constexpr std::size_t long_count = 1000;
constexpr FLOAT long_advance = 10;
constexpr FLOAT long_width = 400;
constexpr FLOAT long_origin = -3000;
constexpr FLOAT long_scroll = 3000;
constexpr std::size_t long_first_inside = 300;
constexpr std::size_t long_inside = 40;
constexpr UINT32 long_text = 5;
using Run = std::optional<win32::BodyGlyphRun>;
using Clusters = std::vector<UINT16>;
using Offsets = std::vector<DWRITE_GLYPH_OFFSET>;
// run の bidi の段と、画面の左端（横に送った量）。
using Placement = std::pair<UINT32, FLOAT>;

// 字形の番号を字形の欄に入れる。
std::vector<UINT16> long_indices()
{
    std::vector<UINT16> indices;
    for (std::size_t glyph = 0; glyph < long_count; ++glyph)
    {
        indices.push_back(static_cast<UINT16>(glyph));
    }
    return indices;
}

// 1 文字が glyphs 字形の塊になる cluster map。
Clusters long_clusters(std::size_t glyphs)
{
    Clusters clusters;
    for (std::size_t glyph = 0; glyph < long_count; glyph += glyphs)
    {
        clusters.push_back(static_cast<UINT16>(glyph));
    }
    return clusters;
}

// 画面の左を left にずらした collector では、同じ字形が同じ所に来るように原点も left ずらす。
Run long_kept(IDWriteFontFace *face, const Clusters &clusters, const Offsets &offsets,
              Placement placement = {0, 0.0F})
{
    const auto [bidi, left] = placement;
    const auto indices = long_indices();
    const std::vector<FLOAT> advances(long_count, long_advance);
    const Collector collector = Microsoft::WRL::Make<win32::BodyGlyphCollector>(long_width, left);
    const DWRITE_GLYPH_RUN run{face,
                               run_em,
                               static_cast<UINT32>(long_count),
                               indices.data(),
                               advances.data(),
                               offsets.empty() ? nullptr : offsets.data(),
                               FALSE,
                               bidi};
    const DWRITE_GLYPH_RUN_DESCRIPTION description{
        L"", nullptr, static_cast<UINT32>(clusters.size()), clusters.data(), long_text};
    static_cast<void>(collector->DrawGlyphRun(nullptr, long_origin + left, 0,
                                              DWRITE_MEASURING_MODE_NATURAL, &run,
                                              clusters.empty() ? nullptr : &description, nullptr));
    auto taken = collector->take();
    if (!taken.has_value() || taken.value().size() != 1)
    {
        return std::nullopt;
    }
    return std::move(taken.value().front());
}

// 描く窓が画面の内側の 40 字形をすべて含み、両端は保守の幅（字形の箱の最大・offset の最大・2）に
// 掛かる字形までしか含まない。字形の配列は全体のまま。
bool trimmed_to_screen(const win32::BodyGlyphRun &run, FLOAT reach)
{
    const auto spare = static_cast<std::size_t>(std::ceil(reach / long_advance)) + 1;
    return run.indices.size() == long_count && run.advances.size() == long_count &&
           run.first <= long_first_inside && run.last >= long_first_inside + long_inside &&
           run.last <= long_count && run.last - run.first <= long_inside + 2 * spare;
}

// 窓の先頭の字形を描く原点が、全体を描いたときのその字形の位置（原点から送りを左から積む）と同じ。
bool same_positions(const win32::BodyGlyphRun &run, FLOAT shift = 0)
{
    FLOAT pen = long_origin + shift;
    for (std::size_t glyph = 0; glyph < run.first; ++glyph)
    {
        pen += long_advance;
    }
    return run.origin.x == long_origin + shift &&
           win32::drawn_origin(run, D2D1::Point2F(0, 0)).x == pen;
}

// 文字の位置・cluster map・送りの和は切る前のまま（置き換えの描画 #538 が引く対応）。
bool whole_text(const win32::BodyGlyphRun &run, std::size_t glyphs)
{
    return run.text_position == long_text && run.clusters == long_clusters(glyphs) &&
           run.advance == long_advance * static_cast<FLOAT>(long_count);
}

// DrawGlyphRun へ渡す形は窓の字形・送り・offset を同じ位置から指す。
bool drawn_window(const win32::BodyGlyphRun &run)
{
    const auto glyphs = win32::drawn_glyphs(run);
    const auto count = run.last - run.first;
    return glyphs.glyphCount == count &&
           glyphs.glyphIndices == std::span(run.indices).subspan(run.first).data() &&
           glyphs.glyphAdvances == std::span(run.advances).subspan(run.first).data() &&
           (run.offsets.empty()
                ? glyphs.glyphOffsets == nullptr
                : glyphs.glyphOffsets == std::span(run.offsets).subspan(run.first).data());
}

FLOAT reach_of(const DWRITE_FONT_METRICS1 &metrics, FLOAT offset)
{
    const float extent = std::max(std::abs(static_cast<float>(metrics.glyphBoxLeft)),
                                  std::abs(static_cast<float>(metrics.glyphBoxRight)));
    return extent * run_em / static_cast<float>(metrics.designUnitsPerEm) + 2 + offset;
}

void verify_trimmed_offsets(IDWriteFontFace1 &face, const DWRITE_FONT_METRICS1 &metrics)
{
    Offsets offsets(long_count, DWRITE_GLYPH_OFFSET{0, 0});
    for (std::size_t glyph = 0; glyph < long_count; glyph += 7)
    {
        offsets.at(glyph) = DWRITE_GLYPH_OFFSET{-15, 3};
    }
    const Run run = long_kept(&face, long_clusters(1), offsets);
    expect(run.has_value() && run.value().offsets.size() == long_count &&
               trimmed_to_screen(run.value(), reach_of(metrics, 15)) && drawn_window(run.value()),
           "a long run with glyph offsets passes the offsets of the window");
    const Run unmapped = long_kept(&face, Clusters{}, Offsets{});
    expect(unmapped.has_value() && unmapped.value().offsets.empty() &&
               unmapped.value().clusters.empty() &&
               trimmed_to_screen(unmapped.value(), reach_of(metrics, 0)) &&
               drawn_window(unmapped.value()) && same_positions(unmapped.value()),
           "a long run without offsets or a cluster map is narrowed by the glyphs alone");
}

void verify_trimmed_runs(IDWriteFontFace1 &face)
{
    DWRITE_FONT_METRICS1 metrics{};
    face.GetMetrics(&metrics);
    const FLOAT reach = reach_of(metrics, 0);
    const Run run = long_kept(&face, long_clusters(1), Offsets{});
    expect(run.has_value() && trimmed_to_screen(run.value(), reach),
           "a long run draws only the glyphs near the screen");
    expect(run.has_value() && same_positions(run.value()),
           "the window starts where its first glyph was before the trim");
    expect(run.has_value() && whole_text(run.value(), 1) && drawn_window(run.value()),
           "the text position and the cluster map stay whole and drawing passes the window");
    const Run paired = long_kept(&face, long_clusters(2), Offsets{});
    expect(paired.has_value() && trimmed_to_screen(paired.value(), reach) &&
               same_positions(paired.value()) && whole_text(paired.value(), 2),
           "a run of two-glyph clusters keeps its cluster map whole");
    const Run scrolled = long_kept(&face, long_clusters(1), Offsets{}, {0, long_scroll});
    expect(scrolled.has_value() && run.has_value() && scrolled.value().first == run.value().first &&
               scrolled.value().last == run.value().last &&
               same_positions(scrolled.value(), long_scroll),
           "a scrolled screen draws the same glyphs");
    const Run right_to_left = long_kept(&face, long_clusters(1), Offsets{}, {1, 0.0F});
    expect(right_to_left.has_value() && right_to_left.value().first == 0 &&
               right_to_left.value().last == long_count && whole_text(right_to_left.value(), 1),
           "a right-to-left run is not narrowed (the window is the whole run)");
    verify_trimmed_offsets(face, metrics);
}

// #586 の工程 3: 隣り合う同じ face の run は、後の run の原点が前の run の原点から送りを左から
// float で積んだ右端にちょうど等しいときだけ 1 つにつながる（ADR 0073 の 2026-10-16 の決定）。
// 1 run は 10 字形・送り 7.3（和が float で丸まる値）で、1 字形 1 文字。
constexpr std::size_t joined_count = 10;
constexpr FLOAT joined_advance = 7.3F;
constexpr FLOAT joined_origin = 11.1F;
constexpr FLOAT joined_width = 400;

// 1 つの collector に順に渡す run の 1 つ: face・原点の x・bidi の段・文字の位置。
using JoinedPiece = std::tuple<IDWriteFontFace *, FLOAT, UINT32, UINT32>;

// 原点 x から送りを左から積んだ右端（collector が比べる値）。
FLOAT pen_after(FLOAT x)
{
    for (std::size_t glyph = 0; glyph < joined_count; ++glyph)
    {
        x += joined_advance;
    }
    return x;
}

std::vector<win32::BodyGlyphRun> joined_runs(std::span<const JoinedPiece> pieces)
{
    const Collector collector = Microsoft::WRL::Make<win32::BodyGlyphCollector>(joined_width);
    const std::vector<UINT16> indices(joined_count, 3);
    const std::vector<FLOAT> advances(joined_count, joined_advance);
    std::vector<UINT16> clusters;
    for (std::size_t glyph = 0; glyph < joined_count; ++glyph)
    {
        clusters.push_back(static_cast<UINT16>(glyph));
    }
    for (const auto &[face, x, bidi, text_position] : pieces)
    {
        const DWRITE_GLYPH_RUN run{face,
                                   run_em,
                                   static_cast<UINT32>(joined_count),
                                   indices.data(),
                                   advances.data(),
                                   nullptr,
                                   FALSE,
                                   bidi};
        const DWRITE_GLYPH_RUN_DESCRIPTION description{
            L"", nullptr, static_cast<UINT32>(clusters.size()), clusters.data(), text_position};
        static_cast<void>(collector->DrawGlyphRun(nullptr, x, 0, DWRITE_MEASURING_MODE_NATURAL,
                                                  &run, &description, nullptr));
    }
    return collector->take().value_or(std::vector<win32::BodyGlyphRun>{});
}

// 3 つがつながった run: 字形・送り・文字の対応が順に並び、cluster map は前の字形の数だけ進む。
bool whole_join(const std::vector<win32::BodyGlyphRun> &runs)
{
    if (runs.size() != 1)
    {
        return false;
    }
    const auto &run = runs.front();
    FLOAT advance = 0;
    bool clusters = run.clusters.size() == joined_count * 3;
    for (std::size_t glyph = 0; glyph < run.advances.size(); ++glyph)
    {
        advance += run.advances.at(glyph);
        clusters = clusters && static_cast<std::size_t>(run.clusters.at(glyph)) == glyph;
    }
    return clusters && run.origin.x == joined_origin && run.text_position == 0 &&
           run.indices.size() == joined_count * 3 && run.advance == advance && run.first == 0 &&
           run.last == joined_count * 3 &&
           win32::drawn_origin(run, D2D1::Point2F(0, 0)).x == joined_origin;
}

void verify_joined_runs(IDWriteFontFace1 &face, IDWriteFontFace1 &other)
{
    const FLOAT second = pen_after(joined_origin);
    const FLOAT third = pen_after(second);
    const UINT32 next_text = joined_count;
    const std::array<JoinedPiece, 3> exact{JoinedPiece{&face, joined_origin, 0, 0},
                                           JoinedPiece{&face, second, 0, next_text},
                                           JoinedPiece{&face, third, 0, next_text * 2}};
    expect(whole_join(joined_runs(exact)), "three adjacent runs of the same face are joined");
    auto shifted = exact;
    const FLOAT moved = std::nextafter(second, std::numeric_limits<FLOAT>::infinity());
    std::get<1>(shifted.at(1)) = moved;
    std::get<1>(shifted.at(2)) = pen_after(moved);
    const auto apart = joined_runs(shifted);
    expect(apart.size() == 2 && apart.front().indices.size() == joined_count &&
               apart.back().indices.size() == joined_count * 2,
           "a run one ulp away from the pen is not joined (the next exact one is)");
    auto faces = exact;
    std::get<0>(faces.at(1)) = &other;
    expect(joined_runs(faces).size() == 3, "a run of a different face is not joined");
    auto right_to_left = exact;
    for (auto &piece : right_to_left)
    {
        std::get<2>(piece) = 1;
    }
    expect(joined_runs(right_to_left).size() == 3, "right-to-left runs are not joined");
    auto gap = exact;
    std::get<3>(gap.at(1)) = next_text + 1;
    std::get<3>(gap.at(2)) = next_text * 2 + 1;
    expect(joined_runs(gap).size() == 2, "a run whose text does not continue is not joined");
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

Microsoft::WRL::ComPtr<IDWriteFontFace1> face_at(IDWriteFactory2 &factory, UINT32 index)
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
    Microsoft::WRL::ComPtr<IDWriteFont> font;
    Microsoft::WRL::ComPtr<IDWriteFontFace> face;
    Microsoft::WRL::ComPtr<IDWriteFontFace1> face1;
    if (FAILED(factory.GetSystemFontCollection(collection.GetAddressOf(), FALSE)) ||
        collection->GetFontFamilyCount() <= index ||
        FAILED(collection->GetFontFamily(index, family.GetAddressOf())) ||
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
    const auto face = face_at(*factory.Get(), 0);
    if (face == nullptr)
    {
        unmeasured("the outside bound needs an installed font face");
        return;
    }
    verify_outside_bounds(*face.Get());
    verify_vertical_bounds(*face.Get());
    verify_trimmed_runs(*face.Get());
    const auto other = face_at(*factory.Get(), 1);
    if (other == nullptr)
    {
        unmeasured("joining runs of different faces needs two installed font families");
        return;
    }
    verify_joined_runs(*face.Get(), *other.Get());
}
} // namespace nenenib::tests::ui
