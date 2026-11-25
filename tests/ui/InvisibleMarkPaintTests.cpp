// 不可視文字の印（ADR 0193 の決定 2・5・10・16 の段 2・#1977）。本物の DirectWrite の layout
// とメモリの bitmap で、印の升の x
// が保持した字形から引いて字の位置（HitTestTextRange）と合うこと（等幅と比例の
// 字体・空白・Tab・続く Tab・行末の Tab・全角空白）、保持で位置を引けない行は nullopt で
// HitTestTextRange の道に戻ること、印の画素（点・Tab の線と矢じり・全角空白の輪郭・LF と CRLF
// の改行の 印）、選択の改行の面の上に改行の印が見えることを確かめる（窓・device
// は作らない）。字体に結果が依る ので環境依存（QLT-013）。
#include "BodyGlyphRun.hpp"
#include "BodyTextLayouts.hpp"
#include "InvisibleBox.hpp"
#include "InvisibleMark.hpp"
#include "InvisibleMarkBoxes.hpp"
#include "InvisibleMarkPainter.hpp"
#include "InvisibleMarkUnits.hpp"
#include "LineBreakPlace.hpp"
#include "SoftwareTarget.hpp"
#include "TextLayoutPoint.hpp"
#include "WindowChecks.hpp"

#include "CaretAffinity.hpp"
#include "Column.hpp"
#include "DisplayForm.hpp"
#include "DisplayLine.hpp"
#include "DisplayText.hpp"
#include "EditorSettings.hpp"
#include "InvisibleKind.hpp"
#include "InvisibleSpan.hpp"
#include "LineBreakMark.hpp"
#include "LineInvisibles.hpp"
#include "LineView.hpp"
#include "LineWrapping.hpp"
#include "TextLayoutRequest.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using nenenib::core::InvisibleKind;
using Pixels = SoftwareTarget::Pixels;

constexpr std::int32_t line_height = 22;
constexpr std::int32_t layout_width = 1200;
constexpr std::size_t channels = 4;

[[nodiscard]] bool lit(const Pixels &pixels, std::int32_t x, std::int32_t y)
{
    if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(SoftwareTarget::width) ||
        y >= static_cast<std::int32_t>(SoftwareTarget::height))
    {
        return false;
    }
    const auto at =
        ((static_cast<std::size_t>(y) * SoftwareTarget::width) + static_cast<std::size_t>(x)) *
        channels;
    return pixels.at(at) > 40 || pixels.at(at + 1) > 40 || pixels.at(at + 2) > 40;
}

// 箱の中に光る画素がいくつあるか。
[[nodiscard]] int lit_inside(const Pixels &pixels, D2D1_RECT_F box)
{
    int count = 0;
    for (auto y = static_cast<std::int32_t>(box.top); y < static_cast<std::int32_t>(box.bottom);
         ++y)
    {
        for (auto x = static_cast<std::int32_t>(box.left);
             x < static_cast<std::int32_t>(std::ceil(box.right)); ++x)
        {
            count += lit(pixels, x, y) ? 1 : 0;
        }
    }
    return count;
}

// text の UTF-16 の位置 unit の 1 字の当たり矩形（layout の座標）。
[[nodiscard]] std::optional<DWRITE_HIT_TEST_METRICS> hit_of(IDWriteTextLayout *layout, UINT32 unit)
{
    DWRITE_HIT_TEST_METRICS hit{};
    UINT32 count = 0;
    if (FAILED(layout->HitTestTextRange(unit, 1, 0, 0, &hit, 1, &count)) || count != 1)
    {
        return std::nullopt;
    }
    return hit;
}

[[nodiscard]] bool close_by(float left, float right) noexcept
{
    return std::abs(left - right) < 1.0F;
}

// 字体 family の text で、marks の升が HitTestTextRange の矩形と左右・上端で 1 画素以内に合うか。
void check_boxes(std::string_view family, std::string_view text,
                 const std::vector<win32::InvisibleMark> &marks)
{
    win32::BodyTextLayouts layouts;
    auto settings = nenenib::core::default_editor_settings();
    settings.wrapping = nenenib::core::LineWrapping::no_wrap;
    settings.font_family = nenenib::core::DisplayText::parse(family).value();
    const nenenib::application::TextLayoutRequest request{
        text, settings, nenenib::core::TextViewGeometry{layout_width, line_height, 96}};
    const auto layout = layouts.get(request);
    const auto space = layouts.space_width(nenenib::core::DisplayForm::text);
    const auto *entry = layout.has_value() ? layouts.glyphs_of(layout.value().Get()) : nullptr;
    if (entry == nullptr || !entry->glyphs_ready || !space.has_value())
    {
        unmeasured("the invisible marks need a DirectWrite body layout with kept glyphs");
        return;
    }
    const auto stop = layouts.tab_stop_width(nenenib::core::DisplayForm::text);
    std::vector<win32::InvisibleMark> unresolved;
    const auto boxes = win32::invisible_boxes(
        entry->glyphs, marks,
        win32::InvisibleScale{static_cast<float>(line_height), stop.value_or(space.value())},
        unresolved);
    expect(boxes.has_value() && boxes.value().size() == marks.size() && unresolved.empty() &&
               stop == std::optional<float>{space.value() * 8.0F},
           "every mark of a kept line gets a box from the glyph runs at the layout's tab stops");
    bool aligned = boxes.has_value() && boxes.value().size() == marks.size();
    for (std::size_t index = 0; aligned && index < marks.size(); ++index)
    {
        const auto hit = hit_of(layout.value().Get(), marks.at(index).unit);
        const auto &box = boxes.value().at(index).box;
        aligned = hit.has_value() && close_by(box.left, hit.value().left) &&
                  close_by(box.right, hit.value().left + hit.value().width) &&
                  close_by(box.top, hit.value().top) &&
                  boxes.value().at(index).kind == marks[index].kind;
    }
    expect(aligned, "the mark boxes from the glyph runs are the characters' hit test boxes");
}

void check_alignment()
{
    // 「a b<Tab>c　d  」: 空白 1・Tab 3・全角空白 5・行末の空白 7 と 8。
    const std::vector<win32::InvisibleMark> mixed{{1, InvisibleKind::space},
                                                  {3, InvisibleKind::tab},
                                                  {5, InvisibleKind::ideographic_space},
                                                  {7, InvisibleKind::trailing_space},
                                                  {8, InvisibleKind::trailing_space}};
    for (const auto family : {"Cascadia Code", "Segoe UI"})
    {
        check_boxes(family,
                    "a b\tc\xE3\x80\x80"
                    "d  ",
                    mixed);
        // 行頭の続く Tab と、行末の Tab。
        check_boxes(family, "\t\tx\t",
                    {{0, InvisibleKind::tab}, {1, InvisibleKind::tab}, {3, InvisibleKind::tab}});
        // 字の間の Tab が止まりの直前で終わる（短い Tab）。
        check_boxes(family, "abcdefg\th", {{7, InvisibleKind::tab}});
    }
}

// 位置を引けない保持（文字の対応が無い run）は nullopt で、呼び手は HitTestTextRange に戻る。保持
// できる行では、左の字が保持に無い Tab だけが呼び手へ回り、ほかの印は保持から引く（#2047 の R2）。
// 描く窓 [first, last) の外の字形の空白は横に画面外なので升を返さない（R1）。
void check_unpositioned()
{
    const win32::InvisibleScale scale{static_cast<float>(line_height), 64.0F};
    win32::BodyGlyphRun run{};
    run.indices = {1, 2, 3};
    run.advances = {5.0F, 5.0F, 5.0F};
    run.first = 0;
    run.last = 3;
    std::vector<win32::InvisibleMark> unresolved;
    const std::vector<win32::BodyGlyphRun> runs{run};
    const std::vector<win32::InvisibleMark> marks{{0, InvisibleKind::space}};
    expect(!win32::invisible_boxes(runs, marks, scale, unresolved).has_value(),
           "a kept run without a cluster map sends the line to the hit test path");
    auto mapped = run;
    mapped.clusters = {0, 1, 2};
    const std::vector<win32::BodyGlyphRun> positioned{mapped};
    const std::vector<win32::InvisibleMark> mixed{
        {1, InvisibleKind::space}, {5, InvisibleKind::tab}, {2, InvisibleKind::trailing_space}};
    const auto kept = win32::invisible_boxes(positioned, mixed, scale, unresolved);
    expect(kept.has_value() && kept.value().size() == 2 && unresolved.size() == 1 &&
               unresolved.front().unit == 5 && kept.value().at(0).box.left == 5.0F,
           "only the Tab after a character the glyphs do not hold goes to the hit test path");
    unresolved.clear();
    const std::vector<win32::InvisibleMark> outside{{9, InvisibleKind::space}};
    const auto none = win32::invisible_boxes(positioned, outside, scale, unresolved);
    expect(none.has_value() && none.value().empty() && unresolved.empty(),
           "a space the glyphs do not hold is not visible and gets no box");
    auto windowed = mapped;
    windowed.first = 1;
    windowed.last = 2;
    const std::vector<win32::BodyGlyphRun> narrowed{windowed};
    const std::vector<win32::InvisibleMark> spaces{
        {0, InvisibleKind::space}, {1, InvisibleKind::space}, {2, InvisibleKind::space}};
    const auto seen = win32::invisible_boxes(narrowed, spaces, scale, unresolved);
    expect(seen.has_value() && seen.value().size() == 1 && seen.value().front().box.left == 5.0F,
           "spaces whose glyphs are outside the drawn window get no box");
}

// renderer の写し（#2047 の R6）: 本文の桁 → 描画の位置 → UTF-16。`^A` の置き換えの後ろの空白は
// 置き換えの 2 字の後ろ、4 バイトの字の後ろは UTF-16 の 2 単位の後ろ。折り返した行の終わりは最後の
// 画面行。改行の印は幅いっぱいの行では右端に収まるよう寄せ、右の外へ続く行には描かない。
[[nodiscard]] nenenib::application::LineView
line_of(std::string_view text, std::vector<nenenib::core::InvisibleSpan> spans)
{
    nenenib::application::LineView line{};
    line.text = std::string(text);
    line.display = nenenib::core::display_line(text);
    line.invisibles =
        nenenib::application::LineInvisibles{std::move(spans), nenenib::core::LineBreakMark::lf};
    return line;
}

void check_units()
{
    using nenenib::core::Column;
    using nenenib::core::InvisibleSpan;
    // 「a<U+0001> <U+1F600> x」: 空白は桁 3 と 5。描画は「a^A 😀 x」。
    const auto line = line_of("a\x01 \xF0\x9F\x98\x80 x",
                              {InvisibleSpan{Column{3}, Column{4}, InvisibleKind::space},
                               InvisibleSpan{Column{5}, Column{6}, InvisibleKind::space}});
    const auto marks = win32::invisible_marks_of(line);
    expect(marks.size() == 2 && marks.at(0).unit == 3 && marks.at(1).unit == 6 &&
               win32::line_end_unit(line) == 8,
           "the marks land after the ^A replacement and the surrogate pair in UTF-16 units");
    const auto control = line_of("a\x01"
                                 "b",
                                 {InvisibleSpan{Column{2}, Column{3}, InvisibleKind::space}});
    expect(win32::invisible_marks_of(control).empty(), "a replaced character gets no mark");
    expect(win32::line_break_left(100.0F, 10.0F, 200.0F) == std::optional<float>{100.0F} &&
               win32::line_break_left(195.0F, 10.0F, 200.0F) == std::optional<float>{190.0F} &&
               !win32::line_break_left(201.0F, 10.0F, 200.0F).has_value(),
           "a line break mark is pulled inside the right edge and skipped past it");
    win32::BodyTextLayouts layouts;
    auto settings = nenenib::core::default_editor_settings();
    settings.font_family = nenenib::core::DisplayText::parse("Cascadia Code").value();
    const std::string wrapped(300, 'w');
    const nenenib::application::TextLayoutRequest request{
        wrapped, settings, nenenib::core::TextViewGeometry{400, line_height, 96}};
    const auto layout = layouts.get(request);
    DWRITE_TEXT_METRICS metrics{};
    if (!layout.has_value() || FAILED(layout.value()->GetMetrics(&metrics)) ||
        metrics.lineCount < 2)
    {
        unmeasured("the wrapped line break needs a DirectWrite body layout");
        return;
    }
    const auto end =
        win32::text_layout_point(layout.value().Get(), win32::line_end_unit(line_of(wrapped, {})),
                                 nenenib::core::CaretAffinity::trailing);
    const auto last = static_cast<float>(metrics.lineCount - 1) * static_cast<float>(line_height);
    expect(end.has_value() && std::abs(end.value_or(win32::TextLayoutPoint{0, 0}).y - last) < 1.0F,
           "the line break of a wrapped line sits on its last screen row");
}

[[nodiscard]] Pixels painted(SoftwareTarget &target, const win32::InvisibleBox &box)
{
    return target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            win32::InvisibleMarkPainter::paint(context, brush, box,
                                               static_cast<float>(line_height));
        });
}

void check_mark_pixels(SoftwareTarget &target)
{
    const D2D1_RECT_F cell = D2D1::RectF(100, 0, 110, line_height);
    const auto dot = painted(target, win32::InvisibleBox{cell, InvisibleKind::trailing_space});
    expect(lit(dot, 105, line_height / 2) && !lit(dot, 101, 2) && !lit(dot, 105, 2) &&
               lit_inside(dot, cell) < 20,
           "a space mark is a small dot in the middle of its cell");
    const D2D1_RECT_F tab_cell = D2D1::RectF(100, 0, 180, line_height);
    const auto tab = painted(target, win32::InvisibleBox{tab_cell, InvisibleKind::tab});
    expect(lit(tab, 120, line_height / 2) && lit(tab, 170, line_height / 2) &&
               !lit(tab, 101, line_height / 2) && !lit(tab, 140, 3) &&
               lit(tab, 175, (line_height / 2) - 2),
           "a Tab mark is a line across the Tab with an arrow head at its right end");
    const D2D1_RECT_F wide_cell = D2D1::RectF(100, 0, 122, line_height);
    const auto wide =
        painted(target, win32::InvisibleBox{wide_cell, InvisibleKind::ideographic_space});
    expect(lit(wide, 105, line_height / 2) && !lit(wide, 111, line_height / 2) &&
               !lit(wide, 101, 1),
           "an ideographic space mark is the outline of a square inside its cell");
    expect(lit_inside(painted(target, win32::InvisibleBox{D2D1::RectF(300, 0, 300, line_height),
                                                          InvisibleKind::tab}),
                      D2D1::RectF(0, 0, 299, line_height)) == 0,
           "a mark stays inside its cell");
}

[[nodiscard]] Pixels line_break(SoftwareTarget &target, nenenib::core::LineBreakMark mark,
                                bool face)
{
    return target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            if (face)
            {
                // 選択の行末の改行の面（6 DIP）を先に塗り、その上に印を描く（決定 10）。
                brush->SetColor(D2D1::ColorF(0.0F, 0.0F, 0.5F));
                context.FillRectangle(D2D1::RectF(200, 0, 206, line_height), brush);
                brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            }
            win32::InvisibleMarkPainter::paint_line_break(
                context, brush,
                win32::LineBreakPlace{D2D1::Point2F(200, 0), mark, static_cast<float>(line_height),
                                      10.0F});
        });
}

void check_line_breaks(SoftwareTarget &target)
{
    const auto lf = line_break(target, nenenib::core::LineBreakMark::lf, false);
    // ↵ の縦の線は x = 208（y 7〜13）、横の線は y = 13（x 202〜208）。
    expect(lit(lf, 208, 9) && lit(lf, 205, 13) && !lit(lf, 208, 3) && !lit(lf, 214, 9) &&
               lit_inside(lf, D2D1::RectF(210, 0, 230, line_height)) == 0,
           "an LF mark is one bent arrow in one space after the line end");
    const auto crlf = line_break(target, nenenib::core::LineBreakMark::crlf, false);
    // ← は y = 11（x 202〜208）、↵ は 10 右へずれる（縦の線は x = 218）。
    expect(lit(crlf, 205, 11) && lit(crlf, 218, 9) && lit(crlf, 215, 13) && !lit(crlf, 208, 7),
           "a CRLF mark puts a small left arrow before the bent arrow");
    const auto over = line_break(target, nenenib::core::LineBreakMark::lf, true);
    const auto at = [&over](std::int32_t x, std::int32_t y)
    {
        const auto index =
            ((static_cast<std::size_t>(y) * SoftwareTarget::width) + static_cast<std::size_t>(x)) *
            channels;
        return std::array<BYTE, 3>{over.at(index), over.at(index + 1), over.at(index + 2)};
    };
    expect(at(205, 13)[1] > 128 && at(201, 3)[0] > 100 && at(201, 3)[1] < 20,
           "the line break mark is seen over the selection's line break face");
}
} // namespace

void verify_invisible_mark_paint()
{
    SoftwareTarget target;
    if (!target.ready())
    {
        unmeasured("the invisible marks need the software target");
        return;
    }
    check_alignment();
    check_unpositioned();
    check_units();
    check_mark_pixels(target);
    check_line_breaks(target);
}
} // namespace nenenib::tests::ui
