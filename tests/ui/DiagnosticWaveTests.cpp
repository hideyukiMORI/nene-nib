// 診断の波線（ADR 0161 の決定 4・#995 の L1c）。draw_diagnostic_wave が整数の画素に周期 4・
// 振幅 1・太さ 1 の三角波を塗った色のまま引くこと、帯の外を塗らないこと、格子の画面行の
// GridRowPainter が診断の升の下端に本文の error の色で同じ波を引き、診断の無い行には引かない
// ことを、メモリの bitmap（WIC）のソフトウェアの描画先で確かめる。本物の DirectWrite の格子の升は
// フォントに依るので、測れなければ 1 行言う（QLT-013）。
#include "BodyTextLayouts.hpp"
#include "DiagnosticWave.hpp"
#include "GridRowCanvas.hpp"
#include "GridRowPainter.hpp"
#include "SoftwareTarget.hpp"
#include "WaveBand.hpp"
#include "WindowChecks.hpp"

#include "CaretShape.hpp"
#include "CompositionView.hpp"
#include "DiagnosticSeverity.hpp"
#include "DisplayForm.hpp"
#include "EditorSettings.hpp"
#include "GridDiagnosticSpan.hpp"
#include "GridRowView.hpp"
#include "PaneRole.hpp"
#include "RenderFailure.hpp"
#include "RgbColor.hpp"
#include "TextLayoutRequest.hpp"
#include "TextViewGeometry.hpp"
#include "Theme.hpp"

#include <array>
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
namespace win32 = nenenib::ui::win32;
namespace app = nenenib::application;
namespace core = nenenib::core;
using Pixels = SoftwareTarget::Pixels;
using GridPainter = win32::GridRowPainter<ID2D1RenderTarget, win32::BodyTextLayouts>;

constexpr std::int32_t row_height = 40;
constexpr std::int32_t layout_width = 1200;

// 画素 (x, y) の BGRA。
[[nodiscard]] core::RgbColor pixel(const Pixels &pixels, std::size_t x, std::size_t y)
{
    const std::size_t index = ((y * SoftwareTarget::width) + x) * 4;
    return core::RgbColor{pixels.at(index + 2), pixels.at(index + 1), pixels.at(index)};
}

[[nodiscard]] bool black(core::RgbColor color) noexcept
{
    return color.red == 0 && color.green == 0 && color.blue == 0;
}

constexpr std::int32_t wave_left = 10;
constexpr std::int32_t wave_right = 30;
constexpr std::int32_t wave_bottom = 40;
constexpr std::int32_t wave_top = wave_bottom - 3;

// 画素 (x, y) が周期 4・振幅 1・太さ 1 の波で塗られるべきか（位相 0 1 2 3 の高さは 0 1 2 1）。
[[nodiscard]] bool wave_wanted(std::int32_t x, std::int32_t y)
{
    constexpr std::array<std::int32_t, 4> heights{0, 1, 2, 1};
    return x >= wave_left && x < wave_right &&
           y == wave_top + heights.at(static_cast<std::size_t>(x % 4));
}

// 帯の周りの画素を見て、塗るべき画素だけが赤で、ほかが黒か。
[[nodiscard]] bool wave_exact(const Pixels &pixels)
{
    bool exact = true;
    for (std::int32_t x = wave_left - 2; x < wave_right + 2; ++x)
    {
        for (std::int32_t y = wave_top - 2; y < wave_bottom + 2; ++y)
        {
            const auto color =
                pixel(pixels, static_cast<std::size_t>(x), static_cast<std::size_t>(y));
            exact =
                exact && (wave_wanted(x, y) ? color == core::RgbColor{255, 0, 0} : black(color));
        }
    }
    return exact;
}

// 周期 4・振幅 1・太さ 1 の波: 列 x の塗った画素は下端 bottom から上へ 3 画素の帯の中で、高さは
// x の位相 0 1 2 3 に 0 1 2 1。帯の外の列と、列の中の塗っていない画素は黒のまま。
void verify_wave_pixels(SoftwareTarget &target)
{
    constexpr std::int32_t left = wave_left;
    constexpr std::int32_t right = wave_right;
    constexpr std::int32_t bottom = wave_bottom;
    const Pixels pixels = target.painted(
        [](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            brush->SetColor(D2D1::ColorF(1, 0, 0, 1));
            win32::draw_diagnostic_wave(context, brush,
                                        win32::WaveBand{static_cast<float>(left),
                                                        static_cast<float>(right), bottom, 1, 1,
                                                        4});
        });
    if (pixels.empty())
    {
        unmeasured("the diagnostic wave needs a software target");
        return;
    }
    const bool exact = wave_exact(pixels);
    expect(exact, "the wave paints one pixel per column at the triangle height in the brush color");
    expect(win32::wave_offset(-1, 4, 1) == 1 && win32::wave_offset(6, 4, 1) == 2 &&
               win32::wave_offset(7, 6, 2) == 1,
           "the wave phase follows the absolute column and scales with the amplitude");
}

[[nodiscard]] app::GridRowView grid_row(std::vector<app::GridDiagnosticSpan> diagnostics)
{
    std::vector<std::size_t> starts(10);
    for (std::size_t index = 0; index < starts.size(); ++index)
    {
        starts.at(index) = index;
    }
    app::GridRowView row{core::LineNumber{1},
                         0,
                         0,
                         std::string("int value"),
                         std::move(starts),
                         std::nullopt,
                         false,
                         {},
                         std::nullopt,
                         {},
                         std::nullopt,
                         std::nullopt};
    row.diagnostics = std::move(diagnostics);
    return row;
}

// 格子の 1 画面行を製品の GridRowPainter で描く。字は黒（背景と同じ）で、波だけが見える。
[[nodiscard]] Pixels grid_drawn(SoftwareTarget &target, win32::BodyTextLayouts &layouts,
                                const app::GridRowView &row, float cell)
{
    const auto settings = core::default_editor_settings();
    const auto theme = core::selected_theme(settings, core::Appearance::dark);
    auto palette = theme.ui;
    palette.text = core::RgbColor{0, 0, 0};
    const std::optional<app::CompositionView> composition;
    std::optional<win32::RenderFailure> failure;
    const core::TextViewGeometry geometry{layout_width, row_height, 96};
    return target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            GridPainter painter(context, brush, layouts, failure);
            const win32::GridRowCanvas canvas{core::LayoutRect{0, 0, layout_width, row_height},
                                              cell,
                                              settings,
                                              geometry,
                                              palette,
                                              theme.body,
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
                                              1,
                                              1,
                                              4,
                                              win32::PaneRole::active};
            static_cast<void>(painter.draw(row, canvas));
        });
}

// 行の高さの中で、x が [from, to) にある color の画素の数。
[[nodiscard]] std::size_t colored_pixels(const Pixels &pixels, core::RgbColor color, float from,
                                         float to)
{
    std::size_t found = 0;
    for (std::size_t y = 0; y < static_cast<std::size_t>(row_height); ++y)
    {
        for (std::size_t x = 0; x < SoftwareTarget::width; ++x)
        {
            const auto at = static_cast<float>(x);
            found += at >= from && at < to && pixel(pixels, x, y) == color ? 1 : 0;
        }
    }
    return found;
}

// 格子の画面行: 升 [4, 9) の error の下に本文の error の色の画素があり、升の外と診断の無い行には
// error の色の画素が無い。
void verify_grid_wave(SoftwareTarget &target, win32::BodyTextLayouts &layouts)
{
    const auto settings = core::default_editor_settings();
    const auto cell =
        layouts.cell(app::TextLayoutRequest{{},
                                            settings,
                                            core::TextViewGeometry{layout_width, row_height, 96},
                                            core::DisplayForm::grid});
    if (!cell)
    {
        unmeasured("the grid wave needs the DirectWrite grid cell");
        return;
    }
    const auto width = cell.value().width;
    const auto error = core::selected_theme(settings, core::Appearance::dark).body.error;
    const auto count = [&error](const Pixels &pixels, float from, float to)
    { return colored_pixels(pixels, error, from, to); };
    const Pixels marked =
        grid_drawn(target, layouts,
                   grid_row({app::GridDiagnosticSpan{app::GridCellSpan{4, 9},
                                                     core::DiagnosticSeverity::error}}),
                   width);
    const Pixels plain = grid_drawn(target, layouts, grid_row({}), width);
    const float begin = 4 * width;
    const float end = 9 * width;
    expect(!marked.empty() && count(marked, begin, end) > 0,
           "an error's cells are underlined with the body error color");
    expect(count(marked, 0, begin - 1) == 0 &&
               count(marked, end + 1, static_cast<float>(SoftwareTarget::width)) == 0,
           "the wave does not reach outside the diagnostic's cells");
    expect(!plain.empty() && count(plain, 0, static_cast<float>(SoftwareTarget::width)) == 0,
           "a row without diagnostics has no wave");
}
} // namespace

void verify_diagnostic_waves()
{
    SoftwareTarget target;
    if (!target.ready())
    {
        unmeasured("the diagnostic waves need a software target");
        return;
    }
    verify_wave_pixels(target);
    win32::BodyTextLayouts layouts;
    verify_grid_wave(target, layouts);
}
} // namespace nenenib::tests::ui
