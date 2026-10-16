// 置き換えた文字の描き直し（ADR 0040 の決定 4）を、保持した字形 run から引く契約（#531）。
// 1 つの置き換えに描くのは同じ行で横に交わる run だけで、描く run の数は置き換えの数 × 行の
// run の数にならない。製品と同じ BodyTextPainter の draw_replaced → tint_runs → draw_body_text の
// 配線を替え玉の描画先に通して DrawGlyphRun を数える。切り抜きは HitTestTextRange の矩形と
// 同じ所に来る（本物の DirectWrite はフォントに依るので環境依存・QLT-013）。
#include "BodyGlyphCollector.hpp"
#include "BodyTextLayouts.hpp"
#include "BodyTextPainter.hpp"
#include "CountingTarget.hpp"
#include "DirectWriteFactory.hpp"
#include "ReplacedTints.hpp"
#include "WindowChecks.hpp"

#include "EditorSettings.hpp"
#include "RenderFailure.hpp"

#include <array>
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
namespace win32 = nenenib::ui::win32;
using Runs = std::vector<win32::BodyGlyphRun>;
using Ranges = std::vector<DWRITE_TEXT_RANGE>;

constexpr float line_height = 20;
constexpr float baseline = 14;

// 文字と字形が 1 対 1 の run。face は描かないので要らない。
win32::BodyGlyphRun run_at(D2D1_POINT_2F origin, UINT32 text_position, std::size_t glyphs,
                           float advance)
{
    win32::BodyGlyphRun run{nullptr,
                            line_height,
                            FALSE,
                            0,
                            origin,
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

std::size_t drawn_runs(const std::vector<win32::ReplacedTint> &tints)
{
    std::size_t drawn = 0;
    for (const auto &tint : tints)
    {
        drawn += tint.last - tint.first;
    }
    return drawn;
}

// 1 行に、絵文字の run（1 字形）と置き換え `<200d>` の run（6 字形）が交互に並ぶ（家族の行の形）。
void verify_draw_count()
{
    constexpr std::size_t replaced = 200;
    Runs runs;
    Ranges ranges;
    float x = 0;
    UINT32 position = 0;
    for (std::size_t index = 0; index < replaced; ++index)
    {
        runs.push_back(run_at(D2D1::Point2F(x, baseline), position, 1, 20));
        x += 20;
        position += 2;
        runs.push_back(run_at(D2D1::Point2F(x, baseline), position, 6, 8));
        ranges.push_back(DWRITE_TEXT_RANGE{position, 6});
        x += 48;
        position += 6;
    }
    const auto tints = win32::replaced_tints(runs, ranges, line_height);
    expect(tints.has_value() && tints.value().size() == replaced,
           "every replaced range on a positioned line gets one clip");
    // 本文で 1 回ずつ（M）と、置き換えの切り抜きの中で交わる run だけ。隣の run とは重ならない。
    const std::size_t calls = runs.size() + (tints.has_value() ? drawn_runs(tints.value()) : 0);
    expect(calls <= runs.size() + replaced,
           "replaced clips for R ranges and M runs draw within M + R runs (not R x M)");
}

// 家族の絵文字を置き換えの `<200d>` でつないで families 個並べた 1 行（#531 で止まった形）と、
// 置き換えの UTF-16 の範囲。1 家族は 👨 <200d> 👩 <200d> 👧 と空白（2 + 6 + 2 + 6 + 2 + 1）。
std::pair<std::string, Ranges> family_line(std::size_t families)
{
    std::string text;
    Ranges ranges;
    UINT32 position = 0;
    for (std::size_t index = 0; index < families; ++index)
    {
        text += "\xF0\x9F\x91\xA8<200d>\xF0\x9F\x91\xA9<200d>\xF0\x9F\x91\xA7 ";
        ranges.push_back(DWRITE_TEXT_RANGE{position + 2, 6});
        ranges.push_back(DWRITE_TEXT_RANGE{position + 10, 6});
        position += 19;
    }
    return {text, ranges};
}

// 製品と同じ配線（BodyTextPainter の draw_body_text と draw_replaced）で替え玉の描画先に描き、
// DrawGlyphRun を数える。本文で M 回、置き換え R 個では交わる run だけ（隣の run とは重ならない）。
// 置き換えごとに tint_runs（HitTestTextRange と行の全 run）へ戻ると M + R x M 回になって落ちる。
void check_wired(win32::BodyTextLayouts &layouts, IDWriteTextLayout *layout,
                 const nenenib::core::LayoutRect &area, const Ranges &ranges)
{
    CountingTarget target;
    std::optional<win32::RenderFailure> failure;
    win32::BodyTextPainter<CountingTarget> painter(target, nullptr, layouts, failure);
    painter.draw_body_text(layout, area, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    const std::size_t body = target.glyph_runs();
    expect(body > ranges.size() && target.text_layouts() == 0,
           "the family line is drawn from its kept glyph runs");
    painter.draw_replaced(layout, area, ranges);
    const std::size_t replaced = target.glyph_runs() - body;
    expect(replaced >= ranges.size() && target.glyph_runs() <= body + ranges.size() &&
               target.text_layouts() == 0,
           "draw_replaced draws M + R glyph runs for M runs and R replaced ranges (not R x M)");
    expect(target.balanced() && !failure.has_value(),
           "the wired replaced drawing pops every clip it pushes and does not fail");
}

// 折り返さない幅の広い 1 行で、置き換えはどれも 1 つの切り抜きになる（行をまたがない）。
void verify_wired_draw_count()
{
    constexpr std::int32_t width = 20000;
    const auto [text, ranges] = family_line(40);
    win32::BodyTextLayouts layouts;
    auto settings = nenenib::core::default_editor_settings();
    settings.wrapping = nenenib::core::LineWrapping::no_wrap;
    const nenenib::application::TextLayoutRequest request{
        text, settings,
        nenenib::core::TextViewGeometry{width, static_cast<std::int32_t>(line_height), 96}};
    const auto layout = layouts.get(request);
    const auto rows = layouts.rows(request);
    if (!layout.has_value() || !rows.has_value())
    {
        unmeasured("the wired replaced drawing needs a DirectWrite body layout");
        return;
    }
    const nenenib::core::LayoutRect area{0, 0, width,
                                         static_cast<std::int32_t>(rows.value()) *
                                             static_cast<std::int32_t>(line_height)};
    check_wired(layouts, layout.value().Get(), area, ranges);
}

// 切り抜きは run の origin と advances の和から決まり、行の上端は行の高さの倍数。
void verify_clip_positions()
{
    const Runs runs{run_at(D2D1::Point2F(5, baseline), 0, 10, 7)};
    const auto tints = win32::replaced_tints(runs, Ranges{DWRITE_TEXT_RANGE{2, 2}}, line_height);
    expect(tints.has_value() && tints.value().size() == 1 &&
               tints.value().front().clip.left == 19 && tints.value().front().clip.right == 33 &&
               tints.value().front().clip.top == 0 &&
               tints.value().front().clip.bottom == line_height &&
               tints.value().front().first == 0 && tints.value().front().last == 1,
           "a range inside a run clips its own glyphs and draws that run once");
}

// 折り返しで 2 行に分かれた範囲は行ごとの切り抜きになり、それぞれ自分の行の run だけを描く。
void verify_wrapped_range()
{
    const Runs runs{run_at(D2D1::Point2F(0, baseline), 0, 4, 10),
                    run_at(D2D1::Point2F(0, baseline + line_height), 4, 4, 10),
                    run_at(D2D1::Point2F(40, baseline + line_height), 8, 1, 20)};
    const auto tints = win32::replaced_tints(runs, Ranges{DWRITE_TEXT_RANGE{2, 4}}, line_height);
    expect(tints.has_value() && tints.value().size() == 2 && tints.value().at(0).clip.top == 0 &&
               tints.value().at(0).clip.left == 20 && tints.value().at(0).clip.right == 40 &&
               tints.value().at(0).first == 0 && tints.value().at(0).last == 1 &&
               tints.value().at(1).clip.top == line_height && tints.value().at(1).clip.left == 0 &&
               tints.value().at(1).clip.right == 20 && tints.value().at(1).first == 1 &&
               tints.value().at(1).last == 2,
           "a range wrapped over two rows clips each row and draws only that row's runs");
}

// 保持に無い所（縦・横に画面外で集めなかった run）の置き換えは描かない。
void verify_hidden_ranges()
{
    const Runs runs{run_at(D2D1::Point2F(0, baseline), 100, 4, 10),
                    run_at(D2D1::Point2F(40, baseline), 200, 4, 10)};
    const Ranges ranges{DWRITE_TEXT_RANGE{10, 6}, DWRITE_TEXT_RANGE{101, 1},
                        DWRITE_TEXT_RANGE{150, 6}, DWRITE_TEXT_RANGE{300, 6}};
    const auto tints = win32::replaced_tints(runs, ranges, line_height);
    expect(tints.has_value() && tints.value().size() == 1 && tints.value().front().clip.left == 10,
           "replaced ranges outside the kept runs are not drawn");
}

// 位置を引けない保持は今までの経路（HitTestTextRange）へ戻す印を返す。
void verify_unpositioned()
{
    Runs right_to_left{run_at(D2D1::Point2F(0, baseline), 0, 4, 10)};
    right_to_left.front().bidi = 1;
    expect(!win32::replaced_tints(right_to_left, Ranges{DWRITE_TEXT_RANGE{0, 1}}, line_height)
                .has_value(),
           "a right-to-left run falls back to hit testing");
    Runs unmapped{run_at(D2D1::Point2F(0, baseline), 0, 4, 10)};
    unmapped.front().clusters.clear();
    expect(
        !win32::replaced_tints(unmapped, Ranges{DWRITE_TEXT_RANGE{0, 1}}, line_height).has_value(),
        "a run without the cluster map falls back to hit testing");
    const Runs plain{run_at(D2D1::Point2F(0, baseline), 0, 4, 10)};
    expect(!win32::replaced_tints(plain, Ranges{DWRITE_TEXT_RANGE{2, 1}, DWRITE_TEXT_RANGE{0, 1}},
                                  line_height)
                .has_value(),
           "unsorted ranges fall back to hit testing");
}

using Layout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;

std::optional<Runs> collected(IDWriteTextLayout *layout, float width)
{
    const auto collector = Microsoft::WRL::Make<win32::BodyGlyphCollector>(width);
    if (collector == nullptr || FAILED(layout->Draw(nullptr, collector.Get(), 0, 0)))
    {
        return std::nullopt;
    }
    return collector->take();
}

std::vector<DWRITE_HIT_TEST_METRICS> hit_rectangles(IDWriteTextLayout *layout,
                                                    DWRITE_TEXT_RANGE range)
{
    UINT32 count = 0;
    static_cast<void>(
        layout->HitTestTextRange(range.startPosition, range.length, 0, 0, nullptr, 0, &count));
    std::vector<DWRITE_HIT_TEST_METRICS> rectangles(count);
    if (FAILED(layout->HitTestTextRange(range.startPosition, range.length, 0, 0, rectangles.data(),
                                        count, &count)))
    {
        return {};
    }
    rectangles.resize(count);
    return rectangles;
}

bool close_to(float left, float right)
{
    return std::abs(left - right) < 0.001F;
}

// 本物の layout で、切り抜きが HitTestTextRange
// の矩形と同じ所に来るか（字体の切り替えと折り返し）。
bool same_as_hit_test(IDWriteFactory2 &factory, IDWriteTextFormat *format, float width)
{
    const std::wstring text = L"a\U0001F468<200d>\U0001F469<200d>\U0001F467 x^My";
    const Ranges ranges{DWRITE_TEXT_RANGE{3, 6}, DWRITE_TEXT_RANGE{11, 6},
                        DWRITE_TEXT_RANGE{21, 2}};
    Layout layout;
    if (FAILED(factory.CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format,
                                        width, line_height * 4, layout.GetAddressOf())))
    {
        return false;
    }
    const auto runs = collected(layout.Get(), width);
    const auto tints = runs.has_value()
                           ? win32::replaced_tints(runs.value(), ranges, line_height + 10)
                           : std::nullopt;
    std::vector<DWRITE_HIT_TEST_METRICS> expected;
    for (const auto &range : ranges)
    {
        const auto rectangles = hit_rectangles(layout.Get(), range);
        expected.insert(expected.end(), rectangles.begin(), rectangles.end());
    }
    if (!tints.has_value() || tints.value().size() != expected.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        const auto &clip = tints.value().at(index).clip;
        const auto &hit = expected.at(index);
        if (!close_to(clip.left, hit.left) || !close_to(clip.right, hit.left + hit.width) ||
            !close_to(clip.top, hit.top) || !close_to(clip.bottom, hit.top + hit.height))
        {
            return false;
        }
    }
    return true;
}

void verify_real_layout()
{
    const auto factory = directwrite_factory();
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    if (factory == nullptr ||
        FAILED(factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                         line_height, L"", format.GetAddressOf())) ||
        FAILED(format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, line_height + 10,
                                      baseline + 5)))
    {
        unmeasured("the replaced clips need a DirectWrite text format");
        return;
    }
    expect(same_as_hit_test(*factory.Get(), format.Get(), 1000),
           "replaced clips on one row match the HitTestTextRange rectangles");
    expect(same_as_hit_test(*factory.Get(), format.Get(), 90),
           "replaced clips on wrapped rows match the HitTestTextRange rectangles");
}
} // namespace

void verify_replaced_tints()
{
    verify_draw_count();
    verify_wired_draw_count();
    verify_clip_positions();
    verify_wrapped_range();
    verify_hidden_ranges();
    verify_unpositioned();
    verify_real_layout();
}
} // namespace nenenib::tests::ui
