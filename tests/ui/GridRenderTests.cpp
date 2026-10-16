// 格子の描画（ADR 0140 の決定 5・#546）の契約。製品と同じ GridRowPainter を替え玉の描画先
// （CountingTarget・#531 の形）に通し、見えている画面行の数だけ layout を作ること、塗る矩形が
// 升の範囲から来ること（画面行をまたぐ当たりは 2 つ）を数える。字形が升の左端に立つこと（幅 2 は
// 2 升・幅 0 は前の升）は本物の DirectWrite の layout
// で測る（フォントに依るので環境依存・QLT-013）。
#include "BodyTextLayouts.hpp"
#include "CountingTarget.hpp"
#include "GridCountingLayouts.hpp"
#include "GridRowPainter.hpp"
#include "WindowChecks.hpp"

#include "EditorSettings.hpp"
#include "GridRowView.hpp"
#include "RenderFailure.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
namespace win32 = nenenib::ui::win32;
using Painter = win32::GridRowPainter<CountingTarget, GridCountingLayouts>;

constexpr std::int32_t row_height = 20;
constexpr core::TextViewGeometry geometry{800, row_height, 96};
constexpr float tolerance = 0.5F;

[[nodiscard]] std::vector<std::size_t> counted(std::size_t count)
{
    std::vector<std::size_t> starts(count + 1);
    for (std::size_t index = 0; index <= count; ++index)
    {
        starts.at(index) = index;
    }
    return starts;
}

[[nodiscard]] app::GridRowView row_of(std::size_t row, std::string text,
                                      std::vector<std::size_t> starts)
{
    return app::GridRowView{core::LineNumber{1},
                            row,
                            static_cast<std::int64_t>(row),
                            std::move(text),
                            std::move(starts),
                            std::nullopt,
                            false,
                            {},
                            std::nullopt,
                            {},
                            std::nullopt,
                            std::nullopt};
}

[[nodiscard]] bool close_to(float left, float right) noexcept
{
    return std::abs(left - right) <= tolerance;
}

// 1 行の描画を 1 回。area の上端は画面行の位置。
std::optional<RECT> painted(Painter &painter, const app::GridRowView &row, float cell,
                            const core::EditorSettings &settings)
{
    const auto theme = core::selected_theme(settings, core::Appearance::dark);
    const std::optional<app::CompositionView> composition;
    const auto top = static_cast<std::int32_t>(row.row) * row_height;
    const win32::GridRowCanvas canvas{core::LayoutRect{0, top, geometry.width, top + row_height},
                                      cell,
                                      settings,
                                      geometry,
                                      theme.ui,
                                      core::CaretShape::block,
                                      true,
                                      composition,
                                      2,
                                      2,
                                      6,
                                      1.0F,
                                      2,
                                      1,
                                      1.0F};
    return painter.draw(row, canvas);
}

// 見えている画面行ごとに layout を 1 つ（決定 5）。行の全体を組まない。
void verify_layout_per_row(win32::BodyTextLayouts &layouts, float cell,
                           const core::EditorSettings &settings)
{
    CountingTarget target;
    GridCountingLayouts counting(layouts);
    std::optional<win32::RenderFailure> failure;
    Painter painter(target, nullptr, counting, failure);
    const std::vector<app::GridRowView> rows{
        row_of(0, "aaaaaaaa", counted(8)), row_of(1, "bbbbbbbb", counted(8)),
        row_of(2, "cccccccc", counted(8)), row_of(3, "dd", counted(2))};
    for (const auto &row : rows)
    {
        static_cast<void>(painted(painter, row, cell, settings));
    }
    expect(counting.gets() == rows.size() && !failure.has_value(),
           "the grid draws one layout per visible screen row");
    expect(target.balanced(), "the grid row drawing pops every clip it pushes");
}

// 字形の左端は升の左端（決定 5）。保持した字形の run から数える。
void verify_glyph_lefts(win32::BodyTextLayouts &layouts, float cell,
                        const core::EditorSettings &settings)
{
    CountingTarget target;
    GridCountingLayouts counting(layouts);
    std::optional<win32::RenderFailure> failure;
    Painter painter(target, nullptr, counting, failure);
    static_cast<void>(painted(painter, row_of(0, "ab cd", counted(5)), cell, settings));
    auto lefts = target.glyph_lefts();
    std::ranges::sort(lefts);
    if (target.glyph_runs() == 0 || lefts.size() != 5)
    {
        unmeasured("the grid glyph lefts need kept glyph runs");
        return;
    }
    bool aligned = true;
    for (std::size_t index = 0; index < lefts.size(); ++index)
    {
        aligned = aligned && close_to(lefts.at(index), static_cast<float>(index) * cell);
    }
    expect(aligned, "each glyph of a grid row stands at the left edge of its cell");
}

// 本物の layout の位置で、各 code point の左端が starts の升（幅 2 は 2 升・幅 0 は前の升）。
// expected は (UTF-16 の位置, 升) の組。
[[nodiscard]] bool positioned(win32::BodyTextLayouts &layouts, const app::GridRowView &row,
                              const std::vector<std::pair<UINT32, std::size_t>> &expected,
                              float cell)
{
    const auto settings = core::default_editor_settings();
    const auto layout = layouts.get(
        app::TextLayoutRequest{row.text, settings, geometry, core::DisplayForm::grid, row.starts});
    if (!layout)
    {
        return false;
    }
    bool aligned = true;
    for (const auto &[unit, cells] : expected)
    {
        FLOAT x = 0;
        FLOAT y = 0;
        DWRITE_HIT_TEST_METRICS metrics{};
        aligned = aligned &&
                  SUCCEEDED(layout.value()->HitTestTextPosition(unit, FALSE, &x, &y, &metrics)) &&
                  close_to(x, static_cast<float>(cells) * cell);
    }
    return aligned;
}

void verify_cell_positions(win32::BodyTextLayouts &layouts, float cell)
{
    // 「a日b」: 日 は 2 升なので b は 3 升目。
    expect(positioned(layouts,
                      row_of(0,
                             "a\xE6\x97\xA5"
                             "b",
                             {0, 1, 3, 4}),
                      {{1, 1}, {2, 3}, {3, 4}}, cell),
           "a wide character takes two cells and the next character starts after them");
    // サロゲート対の幅 2（U+20B9F）。
    expect(positioned(layouts,
                      row_of(0,
                             "a\xF0\xA0\xAE\x9F"
                             "b",
                             {0, 1, 3, 4}),
                      {{1, 1}, {3, 3}, {4, 4}}, cell),
           "a wide surrogate pair takes two cells");
    // 結合文字（U+0301）は前の升に重なり、次の文字は 1 升目。
    expect(positioned(layouts, row_of(0, "e\xCC\x81x", {0, 1, 1, 2}), {{2, 1}, {3, 2}}, cell),
           "a combining mark shares the cell of its base");
}

// 画面行をまたぐ当たりは画面行ごとに 1 つの矩形（2 矩形）で、升の範囲から塗る。
void verify_crossing_match(win32::BodyTextLayouts &layouts, float cell,
                           const core::EditorSettings &settings)
{
    CountingTarget target;
    GridCountingLayouts counting(layouts);
    std::optional<win32::RenderFailure> failure;
    Painter painter(target, nullptr, counting, failure);
    auto first = row_of(0, "aaaaaaXY", counted(8));
    first.matches = {app::GridCellSpan{6, 8}};
    first.current_match = app::GridCellSpan{6, 8};
    auto second = row_of(1, "Zaaaa", counted(5));
    second.matches = {app::GridCellSpan{0, 1}};
    second.current_match = app::GridCellSpan{0, 1};
    static_cast<void>(painted(painter, first, cell, settings));
    static_cast<void>(painted(painter, second, cell, settings));
    const auto &fills = target.fills();
    const auto height = static_cast<float>(row_height);
    expect(fills.size() == 2 && close_to(fills.at(0).left, 6 * cell) &&
               close_to(fills.at(0).right, 8 * cell) && close_to(fills.at(0).top, 0) &&
               close_to(fills.at(1).left, 0) && close_to(fills.at(1).right, cell) &&
               close_to(fills.at(1).top, height) && close_to(fills.at(1).bottom, 2 * height),
           "a match over a screen row boundary is painted as two cell rectangles");
    expect(target.outlines().size() == 2, "the current match is outlined on both screen rows");
}

// ブロックのキャレットは升の範囲（幅 2 の文字なら 2 升）。
void verify_block_caret(win32::BodyTextLayouts &layouts, float cell,
                        const core::EditorSettings &settings)
{
    CountingTarget target;
    GridCountingLayouts counting(layouts);
    std::optional<win32::RenderFailure> failure;
    Painter painter(target, nullptr, counting, failure);
    auto row = row_of(0,
                      "a\xE6\x97\xA5"
                      "b",
                      {0, 1, 3, 4});
    row.caret = app::GridCellSpan{1, 3};
    const auto caret = painted(painter, row, cell, settings);
    expect(caret.has_value() && target.fills().size() == 1 &&
               close_to(target.fills().front().left, cell) &&
               close_to(target.fills().front().right, 3 * cell),
           "the block caret on a wide character covers its two cells");
}
} // namespace

void verify_grid_render()
{
    win32::BodyTextLayouts layouts;
    const auto settings = core::default_editor_settings();
    const auto cell =
        layouts.cell(app::TextLayoutRequest{{}, settings, geometry, core::DisplayForm::grid});
    if (!cell)
    {
        unmeasured("the grid drawing needs the DirectWrite grid cell");
        return;
    }
    const auto width = cell.value().width;
    verify_layout_per_row(layouts, width, settings);
    verify_glyph_lefts(layouts, width, settings);
    verify_cell_positions(layouts, width);
    verify_crossing_match(layouts, width, settings);
    verify_block_caret(layouts, width, settings);
}
} // namespace nenenib::tests::ui
