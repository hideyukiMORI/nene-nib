// 構文の色つきの本文の描画（#816・ADR 0156 の決定 11〜13）。保持した字形 run を色の区間の境で
// cluster から切った部分 run の送りの和が元の run と等しく、同じ色で描けば切らずに描いた画素と
// 1 画素も違わないこと、区間の境で色が替わること、保持できない行の切り抜きの道も区間の中だけを
// 塗ることを、替え玉の run とソフトウェアの描画先で確かめる。格子の画面行（#839）も同じ道で、升の
// 範囲から UTF-16 の範囲への写し（幅 2・結合文字）と、区間の境の升で色が替わることを確かめる。
// 本物の DirectWrite の結果はフォントに依るので環境依存（QLT-013）。
#include "BodyColors.hpp"
#include "BodyTextLayouts.hpp"
#include "BodyTextPainter.hpp"
#include "ColoredRun.hpp"
#include "GlyphPieces.hpp"
#include "GridRowCanvas.hpp"
#include "GridRowPainter.hpp"
#include "RefusingGlyphLayouts.hpp"
#include "SoftwareTarget.hpp"
#include "WindowChecks.hpp"

#include "DisplayLine.hpp"
#include "EditorSettings.hpp"
#include "GridRowView.hpp"
#include "PaneRole.hpp"
#include "RenderFailure.hpp"
#include "RgbColor.hpp"
#include "SyntaxPalette.hpp"
#include "SyntaxRole.hpp"
#include "Theme.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Pixels = SoftwareTarget::Pixels;
using Ranges = std::vector<win32::ColoredRun>;

constexpr float line_height = 20;
constexpr std::int32_t layout_width = 1200;
constexpr D2D1_COLOR_F white{1, 1, 1, 1};
constexpr D2D1_COLOR_F green{0, 1, 0, 1};
constexpr D2D1_COLOR_F red{1, 0, 0, 1};
constexpr nenenib::core::LayoutRect area{0, 0, layout_width, 40};

[[nodiscard]] bool same(const D2D1_COLOR_F &left, const D2D1_COLOR_F &right) noexcept
{
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}

// 文字と字形が 1 対 1 の run（ReplacedTintTests と同じ形）。face は描かないので要らない。
win32::BodyGlyphRun run_at(UINT32 text_position, std::size_t glyphs, float advance)
{
    win32::BodyGlyphRun run{nullptr,
                            line_height,
                            FALSE,
                            0,
                            D2D1::Point2F(5, 14),
                            DWRITE_MEASURING_MODE_NATURAL,
                            std::vector<UINT16>(glyphs, 0),
                            std::vector<FLOAT>(glyphs, advance),
                            {},
                            text_position,
                            {},
                            advance * static_cast<float>(glyphs),
                            0,
                            glyphs};
    for (std::size_t index = 0; index < glyphs; ++index)
    {
        run.clusters.push_back(static_cast<UINT16>(index));
    }
    return run;
}

[[nodiscard]] float advances_of(const win32::BodyGlyphRun &run,
                                std::span<const win32::GlyphPiece> pieces)
{
    float sum = 0;
    for (const auto &piece : pieces)
    {
        for (const auto advance :
             std::span(run.advances).subspan(piece.first, piece.last - piece.first))
        {
            sum += advance;
        }
    }
    return sum;
}

// 区間の境で切り、外は plain。部分の送りの和は元の窓と等しく、部分の原点は前の送りを積んだ値。
void verify_pieces()
{
    const auto run = run_at(0, 10, 7);
    const Ranges ranges{{DWRITE_TEXT_RANGE{2, 2}, red}, {DWRITE_TEXT_RANGE{6, 2}, green}};
    const auto pieces = win32::glyph_pieces(run, ranges, white);
    expect(pieces.size() == 5 && pieces.at(0).first == 0 && pieces.at(0).last == 2 &&
               same(pieces.at(0).color, white) && pieces.at(1).first == 2 &&
               pieces.at(1).last == 4 && same(pieces.at(1).color, red) && pieces.at(2).last == 6 &&
               same(pieces.at(2).color, white) && pieces.at(3).last == 8 &&
               same(pieces.at(3).color, green) && pieces.at(4).last == 10 &&
               same(pieces.at(4).color, white),
           "a run is cut at the span borders and the gaps take the plain color");
    expect(advances_of(run, pieces) == run.advance,
           "the pieces' advances add up to the whole run's advance");
    const auto origin = D2D1::Point2F(10, 20);
    expect(win32::drawn_origin(run, origin, 6).x == 10 + 5 + (6 * 7.0F) &&
               win32::drawn_origin(run, origin, 0).x == win32::drawn_origin(run, origin).x,
           "a piece starts where the whole run would have drawn its first glyph");

    auto windowed = run_at(100, 10, 7);
    windowed.first = 3;
    windowed.last = 9;
    const auto clipped =
        win32::glyph_pieces(windowed, Ranges{{DWRITE_TEXT_RANGE{102, 4}, red}}, white);
    expect(clipped.size() == 2 && clipped.at(0).first == 3 && clipped.at(0).last == 6 &&
               same(clipped.at(0).color, red) && clipped.at(1).first == 6 &&
               clipped.at(1).last == 9 && same(clipped.at(1).color, white),
           "pieces stay inside the drawn window of the run");
}

// 区間が run の境をまたぐと run ごとに切れ、cluster の表が無い run は切らずに最初に重なる色。
void verify_crossing_runs()
{
    const auto left = run_at(0, 4, 8);
    const auto right = run_at(4, 4, 8);
    const Ranges across{{DWRITE_TEXT_RANGE{2, 4}, green}};
    const auto first = win32::glyph_pieces(left, across, white);
    const auto second = win32::glyph_pieces(right, across, white);
    expect(first.size() == 2 && first.at(1).first == 2 && same(first.at(1).color, green) &&
               second.size() == 2 && second.at(0).last == 2 && same(second.at(0).color, green),
           "a span over two runs becomes one piece in each run");
    auto bare = run_at(10, 3, 8);
    bare.clusters.clear();
    const auto whole = win32::glyph_pieces(bare, Ranges{{DWRITE_TEXT_RANGE{9, 2}, red}}, white);
    expect(whole.size() == 1 && whole.front().first == 0 && whole.front().last == 3 &&
               same(whole.front().color, red),
           "a run without a cluster map is drawn whole in the first overlapping color");
    auto joined = run_at(0, 3, 8);
    joined.clusters = {0, 0, 1, 2};
    const auto ligature =
        win32::glyph_pieces(joined, Ranges{{DWRITE_TEXT_RANGE{1, 2}, red}}, white);
    expect(ligature.size() == 2 && ligature.at(0).first == 0 && ligature.at(0).last == 2 &&
               same(ligature.at(0).color, red),
           "a border inside a ligature cluster gives the joined glyph to the later span");
}

[[nodiscard]] std::size_t pixel_x(std::size_t index) noexcept
{
    return (index / 4) % SoftwareTarget::width;
}

// 区間の色だけの画素（緑が勝つ）と白の字の画素の最も右・最も左。
[[nodiscard]] std::pair<std::size_t, std::size_t> extents(const Pixels &pixels)
{
    std::size_t green_right = 0;
    std::size_t white_left = SoftwareTarget::width;
    for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
    {
        const auto blue = pixels.at(index);
        const auto shade = pixels.at(index + 1);
        const auto ink = pixels.at(index + 2);
        if (shade > ink && shade > blue)
        {
            green_right = std::max(green_right, pixel_x(index) + 1);
        }
        if (ink > 0 && ink == shade)
        {
            white_left = std::min(white_left, pixel_x(index));
        }
    }
    return {green_right, white_left};
}

template <typename Layouts>
[[nodiscard]] Pixels drawn(SoftwareTarget &target, Layouts &layouts, IDWriteTextLayout *layout,
                           std::span<const win32::ColoredRun> colored)
{
    std::optional<win32::RenderFailure> failure;
    return target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            win32::BodyTextPainter<ID2D1RenderTarget, Layouts> painter(context, brush, layouts,
                                                                       failure);
            painter.draw_colored_body_text(layout, area, win32::BodyColors{colored, white},
                                           [brush](const D2D1_COLOR_F &color)
                                           { brush->SetColor(color); });
        });
}

void check_pixels(SoftwareTarget &target, win32::BodyTextLayouts &layouts,
                  IDWriteTextLayout *layout, float border)
{
    RefusingGlyphLayouts refusing;
    const Pixels plain = drawn(target, layouts, layout, {});
    const auto *entry = layouts.glyphs_of(layout);
    expect(entry != nullptr && entry->glyphs_ready && !entry->glyphs.empty(),
           "the sample line keeps its glyph runs so the cut path is the one compared");
    const Ranges whites{{DWRITE_TEXT_RANGE{0, 3}, white},
                        {DWRITE_TEXT_RANGE{4, 5}, white},
                        {DWRITE_TEXT_RANGE{12, 2}, white}};
    expect(!plain.empty() && drawn(target, layouts, layout, whites) == plain,
           "kept glyphs cut into pieces of the text color draw the same pixels as one run");
    expect(drawn(target, refusing, layout, {}) == plain,
           "a line without kept glyphs draws the same pixels when it has no spans");
    const Ranges keyword{{DWRITE_TEXT_RANGE{0, 3}, green}};
    const auto [green_right, white_left] = extents(drawn(target, layouts, layout, keyword));
    expect(green_right > 0 && static_cast<float>(green_right) <= border + 2 &&
               static_cast<float>(white_left) >= border - 1,
           "the color changes at the span border on kept glyphs");
    const auto [clip_right, rest_left] = extents(drawn(target, refusing, layout, keyword));
    expect(clip_right > 0 && static_cast<float>(clip_right) <= border + 1 &&
               static_cast<float>(rest_left) >= border - 1,
           "a line without kept glyphs paints the span only inside its hit-test clip");
}
// 置き換え（`^M`）の展開のある行（ADR 0040）。区間の境が展開した字の前後にあっても、部分の送りの
// 和は run の窓の送りと等しく、text の色で切った画素は切らない画素と同じ。
void check_replaced_line(SoftwareTarget &target, win32::BodyTextLayouts &layouts,
                         const nenenib::application::TextLayoutRequest &request)
{
    const auto layout = layouts.get(request);
    if (!layout.has_value())
    {
        unmeasured("the replaced line needs a DirectWrite layout");
        return;
    }
    const Pixels plain = drawn(target, layouts, layout.value().Get(), {});
    const auto *entry = layouts.glyphs_of(layout.value().Get());
    // 「x^My = "^M";」: x と ^M・y・"^M" の区間。
    const Ranges spans{{DWRITE_TEXT_RANGE{0, 3}, white},
                       {DWRITE_TEXT_RANGE{3, 1}, white},
                       {DWRITE_TEXT_RANGE{7, 4}, white}};
    bool summed = entry != nullptr && entry->glyphs_ready;
    for (const auto &run :
         entry != nullptr ? std::span(entry->glyphs) : std::span<const win32::BodyGlyphRun>{})
    {
        float window = 0;
        for (const auto advance : std::span(run.advances).subspan(run.first, run.last - run.first))
        {
            window += advance;
        }
        summed = summed && advances_of(run, win32::glyph_pieces(run, spans, white)) == window;
    }
    expect(summed, "pieces cut around an expanded ^M add up to each run's advance");
    expect(!plain.empty() && drawn(target, layouts, layout.value().Get(), spans) == plain,
           "a line with ^M cut into pieces of the text color draws the same pixels");
}
namespace app = nenenib::application;
namespace core = nenenib::core;
using GridPainter = win32::GridRowPainter<ID2D1RenderTarget, win32::BodyTextLayouts>;

[[nodiscard]] app::GridRowView grid_row_of(std::string text, std::vector<std::size_t> starts,
                                           std::vector<app::GridColorSpan> colors)
{
    app::GridRowView row{core::LineNumber{1}, 0,           0,  std::move(text), std::move(starts),
                         std::nullopt,        false,       {}, std::nullopt,    {},
                         std::nullopt,        std::nullopt};
    row.colors = std::move(colors);
    return row;
}

[[nodiscard]] core::SyntaxPalette syntax_palette_of(core::RgbColor others, core::RgbColor type)
{
    auto palette =
        core::SyntaxPalette{others, others, others, others, others, others, others, others,
                            others, others, others, others, others, others, others, others};
    palette.type = type;
    return palette;
}

// 升の範囲 → 画面行の文字列の UTF-16 の範囲。幅 2 の字は 2 升で 1 単位、サロゲート対は 2 単位、
// 結合文字は前の字の升の側に入る。
void verify_grid_color_ranges()
{
    const auto body = syntax_palette_of(core::RgbColor{255, 255, 255}, core::RgbColor{0, 255, 0});
    const auto wide = GridPainter::colored_runs(
        grid_row_of("a\xE6\x97\xA5"
                    "b",
                    {0, 1, 3, 4},
                    {app::GridColorSpan{app::GridCellSpan{1, 3}, core::SyntaxRole::type}}),
        body);
    expect(wide.size() == 1 && wide.front().range.startPosition == 1 &&
               wide.front().range.length == 1,
           "a wide character's two cells are its one UTF-16 unit");
    const auto pair = GridPainter::colored_runs(
        grid_row_of("a\xF0\xA0\xAE\x9F"
                    "b",
                    {0, 1, 3, 4},
                    {app::GridColorSpan{app::GridCellSpan{3, 4}, core::SyntaxRole::type}}),
        body);
    expect(pair.size() == 1 && pair.front().range.startPosition == 3 &&
               pair.front().range.length == 1,
           "a span after a surrogate pair starts after its two UTF-16 units");
    const auto combining = GridPainter::colored_runs(
        grid_row_of("e\xCC\x81x", {0, 1, 1, 2},
                    {app::GridColorSpan{app::GridCellSpan{0, 1}, core::SyntaxRole::type},
                     app::GridColorSpan{app::GridCellSpan{1, 2}, core::SyntaxRole::keyword}}),
        body);
    expect(combining.size() == 2 && combining.at(0).range.length == 2 &&
               combining.at(1).range.startPosition == 2 && combining.at(1).range.length == 1,
           "a combining mark goes with the span of its base cell");
}

// 格子の 1 画面行を製品の GridRowPainter で描く。升の幅は本物の格子の升（測れなければ空）。
[[nodiscard]] Pixels grid_drawn(SoftwareTarget &target, win32::BodyTextLayouts &layouts,
                                const app::GridRowView &row, const core::SyntaxPalette &body)
{
    auto settings = core::default_editor_settings();
    const auto theme = core::selected_theme(settings, core::Appearance::dark);
    auto palette = theme.ui;
    palette.text = core::RgbColor{255, 255, 255};
    const std::optional<app::CompositionView> composition;
    std::optional<win32::RenderFailure> failure;
    const core::TextViewGeometry geometry{layout_width, static_cast<std::int32_t>(line_height), 96};
    const auto measured =
        layouts.cell(app::TextLayoutRequest{{}, settings, geometry, core::DisplayForm::grid});
    if (!measured)
    {
        return {};
    }
    const auto cell = measured.value().width;
    return target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            GridPainter painter(context, brush, layouts, failure);
            const win32::GridRowCanvas canvas{area,
                                              cell,
                                              settings,
                                              geometry,
                                              palette,
                                              body,
                                              core::CaretShape::bar,
                                              false,
                                              composition,
                                              2,
                                              2,
                                              6,
                                              1.0F,
                                              2,
                                              1,
                                              1.0F,
                                              win32::PaneRole::active};
            static_cast<void>(painter.draw(row, canvas));
        });
}

// 格子の画面行（#839）: text の色で塗る区間は区間の無い描画と 1 画素も違わず（部分 run の送りの和が
// 元の run と同じ）、緑の区間は 3 升目の境で白に替わる。
void verify_grid_colored_row(SoftwareTarget &target, win32::BodyTextLayouts &layouts)
{
    const auto cell = layouts.cell(app::TextLayoutRequest{
        {},
        core::default_editor_settings(),
        core::TextViewGeometry{layout_width, static_cast<std::int32_t>(line_height), 96},
        core::DisplayForm::grid});
    if (!cell)
    {
        unmeasured("the colored grid row needs the DirectWrite grid cell");
        return;
    }
    const auto width = cell.value().width;
    std::vector<std::size_t> starts(10);
    for (std::size_t index = 0; index < starts.size(); ++index)
    {
        starts.at(index) = index;
    }
    const std::vector<app::GridColorSpan> spans{
        app::GridColorSpan{app::GridCellSpan{0, 3}, core::SyntaxRole::type},
        app::GridColorSpan{app::GridCellSpan{4, 9}, core::SyntaxRole::keyword}};
    const auto white_body =
        syntax_palette_of(core::RgbColor{255, 255, 255}, core::RgbColor{255, 255, 255});
    const Pixels plain =
        grid_drawn(target, layouts, grid_row_of("int value", starts, {}), white_body);
    expect(!plain.empty() && grid_drawn(target, layouts, grid_row_of("int value", starts, spans),
                                        white_body) == plain,
           "a grid row cut into pieces of the text color draws the same pixels as one run");
    const auto green_body =
        syntax_palette_of(core::RgbColor{255, 255, 255}, core::RgbColor{0, 255, 0});
    const auto [green_right, white_left] =
        extents(grid_drawn(target, layouts, grid_row_of("int value", starts, spans), green_body));
    const auto border = 3 * width;
    expect(green_right > 0 && static_cast<float>(green_right) <= border + 2 &&
               static_cast<float>(white_left) >= border - 1,
           "the grid row's color changes at the cell border of its span");
}
} // namespace

void verify_syntax_render()
{
    verify_pieces();
    verify_crossing_runs();
    verify_grid_color_ranges();
    SoftwareTarget target;
    win32::BodyTextLayouts layouts;
    auto settings = nenenib::core::default_editor_settings();
    settings.wrapping = nenenib::core::LineWrapping::no_wrap;
    const nenenib::application::TextLayoutRequest request{
        "int value = 42;", settings,
        nenenib::core::TextViewGeometry{layout_width, static_cast<std::int32_t>(line_height), 96}};
    const auto layout = layouts.get(request);
    FLOAT x = 0;
    FLOAT y = 0;
    DWRITE_HIT_TEST_METRICS metrics{};
    if (!target.ready() || !layout.has_value() ||
        FAILED(layout.value()->HitTestTextPosition(3, FALSE, &x, &y, &metrics)))
    {
        unmeasured("the colored body text needs a software target and a DirectWrite layout");
        return;
    }
    check_pixels(target, layouts, layout.value().Get(), x);
    const auto replaced = nenenib::core::display_line("x\ry = \"\r\";").text;
    check_replaced_line(
        target, layouts,
        nenenib::application::TextLayoutRequest{replaced, settings, request.geometry});
    verify_grid_colored_row(target, layouts);
}
} // namespace nenenib::tests::ui
