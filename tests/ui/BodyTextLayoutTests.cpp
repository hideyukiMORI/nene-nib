#include "BodyTextLayouts.hpp"
#include "DirectWriteFactory.hpp"
#include "SoftwareTarget.hpp"
#include "SystemFontFamilies.hpp"
#include "WindowChecks.hpp"

#include "EditorSettings.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using nenenib::ui::win32::BodyTextLayouts;
constexpr core::TextViewGeometry narrow{72, 29, 96};

void check_round_trip(BodyTextLayouts &layouts, const app::TextLayoutRequest &request,
                      std::size_t column)
{
    const auto located = layouts.locate(request, core::DisplayCaret{core::Column{column}});
    expect(located.has_value(), "body character has a measured screen position");
    if (!located)
    {
        return;
    }
    const auto hit = layouts.hit(request, located.value());
    expect(hit.has_value() && hit.value().column.value == column,
           "body screen position hits the same character boundary");
}

void check_rows(BodyTextLayouts &layouts, const core::EditorSettings &settings)
{
    const std::string text(180, 'm');
    const app::TextLayoutRequest request{text, settings, narrow};
    const auto count = layouts.rows(request);
    expect(count.has_value() && count.value() > 8, "long body text wraps beyond eight screen rows");
    std::size_t previous = 0;
    for (std::size_t column = 1; column <= text.size() + 1; ++column)
    {
        const auto point = layouts.locate(request, core::DisplayCaret{core::Column{column}});
        expect(point.has_value(), "every ASCII insertion point is located");
        if (!point)
        {
            return;
        }
        expect(point.value().row >= previous, "ASCII screen rows advance monotonically");
        if (point.value().row > previous)
        {
            const auto before = layouts.locate(
                request, core::DisplayCaret{core::Column{column}, core::CaretAffinity::trailing});
            expect(before.has_value() && before.value().row == previous,
                   "one text boundary retains both sides of a wrap");
        }
        previous = point.value().row;
        check_round_trip(layouts, request, column);
    }
    expect(count.has_value() && previous + 1 == count.value(),
           "the last insertion point reaches the last measured screen row");
}

void check_unicode(BodyTextLayouts &layouts, const core::EditorSettings &settings)
{
    constexpr std::array<std::string_view, 3> text{
        "日本語の長い行を折り返してカーソル位置を確かめる。日本語の長い行。", "A\tB\t日本\t語\tend",
        "A😀B😀C😀D😀E😀F😀G😀H"};
    for (const auto value : text)
    {
        const app::TextLayoutRequest request{value, settings, narrow};
        for (std::size_t column = 1; column <= core::code_point_count(value) + 1; ++column)
        {
            check_round_trip(layouts, request, column);
        }
    }
    const app::TextLayoutRequest combining{"áb́ćd́éf́ǵh́íj́", settings, narrow};
    for (std::size_t column = 1; column <= 21; column += 2)
    {
        check_round_trip(layouts, combining, column);
    }
}

void check_resources(BodyTextLayouts &layouts, const core::EditorSettings &settings)
{
    const app::TextLayoutRequest request{"Japanese 日本語 long body text", settings, narrow};
    const auto first = layouts.get(request);
    const auto again = layouts.get(request);
    expect(first.has_value() && again.has_value() && first.value() == again.value(),
           "measurement and drawing share a single complete layout");
    layouts.begin_frame();
    const auto next = layouts.get(request);
    expect(next.has_value() && first.has_value() && next.value() == first.value(),
           "the previous frame's exact layout is retained");
    layouts.end_frame();
    const auto wider = layouts.get(app::TextLayoutRequest{request.text, settings, {900, 29, 96}});
    expect(wider.has_value() && first.has_value() && wider.value() != first.value(),
           "a changed width invalidates body geometry");
    auto no_wrap = settings;
    no_wrap.wrapping = core::LineWrapping::no_wrap;
    const auto single = layouts.rows(app::TextLayoutRequest{request.text, no_wrap, narrow});
    expect(single.has_value() && single.value() == 1, "no-wrap retains one logical screen row");
    const auto wrapped = layouts.rows(request);
    expect(wrapped.has_value() && wrapped.value() > 1,
           "changing wrapping cannot reuse no-wrap geometry");
    const auto layout = layouts.get(request);
    if (layout)
    {
        const auto *glyphs = layouts.glyphs_of(layout.value().Get());
        expect(glyphs != nullptr && glyphs->glyphs_ready && !glyphs->glyphs.empty(),
               "the shared wrapped layout retains its two-dimensional glyph positions");
    }
}

void check_input_retention(BodyTextLayouts &layouts, const core::EditorSettings &settings)
{
    const auto request = app::TextLayoutRequest{"one retained input", settings, narrow};
    const auto first = layouts.get(request);
    const auto second = layouts.get(app::TextLayoutRequest{"another input", settings, narrow});
    const auto replaced = layouts.get(request);
    expect(first && second && replaced && first.value() != replaced.value(),
           "consecutive inputs retain only the latest pending layout");
    layouts.begin_frame();
    const auto shown = layouts.get(request);
    layouts.end_frame();
    expect(shown && replaced && shown.value() == replaced.value(),
           "the latest pending input becomes the exact drawn layout");
    layouts.begin_frame();
    const auto empty = layouts.get(app::TextLayoutRequest{"", settings, narrow});
    layouts.end_frame();
    const auto expired = layouts.get(request);
    expect(empty && expired && shown && expired.value() != shown.value(),
           "a line absent from the next frame is released from the cache");
}

void check_wrapped_pixel_case(BodyTextLayouts &layouts, const app::TextLayoutRequest &request,
                              SoftwareTarget &target)
{
    const auto layout = layouts.get(request);
    expect(layout.has_value(), "the shared wrapped layout is available for pixel comparison");
    if (!layout)
    {
        return;
    }
    const auto *entry = layouts.glyphs_of(layout.value().Get());
    expect(entry != nullptr && entry->glyphs_ready,
           "wrapped glyph collection is retained by the shared body cache");
    if (entry == nullptr || !entry->glyphs_ready)
    {
        return;
    }
    constexpr D2D1_RECT_F clip{7, 9, 79, 67};
    for (const auto row : {0, 1, 3})
    {
        const D2D1_POINT_2F origin{7, static_cast<float>(9 - row * narrow.line_height)};
        const auto expected = target.clipped_layout(layout.value().Get(), origin, clip);
        const auto drawn = target.glyph_runs(entry->glyphs, origin, clip);
        expect(!expected.empty() && expected == drawn,
               "wrapped and partly hidden rows retain every DirectWrite pixel");
    }
}

void check_wrapped_pixels(BodyTextLayouts &layouts, const core::EditorSettings &settings)
{
    SoftwareTarget target;
    expect(target.ready(), "wrapped glyph verification creates an offscreen software target");
    if (!target.ready())
    {
        return;
    }
    constexpr std::array<std::string_view, 3> texts{
        "日本語の長い行を折り返して日本語の続きを表示する。",
        "abc\tdef Café á 😀 漢字 אבג العربية end abcdefghijklmnop",
        "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"};
    for (const auto text : texts)
    {
        check_wrapped_pixel_case(layouts, app::TextLayoutRequest{text, settings, narrow}, target);
    }
}

void check_horizontal_pixel_case(BodyTextLayouts &layouts, const app::TextLayoutRequest &request,
                                 SoftwareTarget &target)
{
    const auto layout = layouts.get(request);
    expect(layout.has_value(), "a complete unwrapped layout is available for horizontal scrolling");
    if (!layout)
    {
        return;
    }
    constexpr D2D1_RECT_F clip{7, 9, 79, 67};
    for (const auto left : {0, 125, 800, 0})
    {
        const auto *entry = layouts.glyphs_of(layout.value().Get(), static_cast<float>(left));
        expect(entry != nullptr && entry->glyphs_ready,
               "visible glyphs are collected at the requested horizontal position");
        if (entry == nullptr || !entry->glyphs_ready)
        {
            return;
        }
        const D2D1_POINT_2F origin{static_cast<float>(7 - left), 9};
        const auto expected = target.clipped_layout(layout.value().Get(), origin, clip);
        const auto drawn = target.glyph_runs(entry->glyphs, origin, clip);
        expect(!expected.empty() && expected == drawn,
               "horizontal glyph collection and returning left retain every DirectWrite pixel");
    }
}

void check_horizontal_pixels(BodyTextLayouts &layouts, core::EditorSettings settings)
{
    settings.wrapping = core::LineWrapping::no_wrap;
    SoftwareTarget target;
    expect(target.ready(), "horizontal glyph verification creates an offscreen software target");
    if (!target.ready())
    {
        return;
    }
    constexpr std::array<std::string_view, 3> texts{
        "日本語の長い行を折り返して日本語の続きを表示する。",
        "abc\tdef Café á 😀 漢字 אבג العربية end abcdefghijklmnop",
        "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"};
    for (const auto text : texts)
    {
        std::string repeated;
        for (int index = 0; index < 6; ++index)
        {
            repeated += text;
        }
        check_horizontal_pixel_case(layouts, app::TextLayoutRequest{repeated, settings, narrow},
                                    target);
    }
}

// 格子の 1 升（ADR 0140）。格子の書式の `0` の幅と行高で、書式が同じなら同じ値。
// 保持の行（rows の要求）には入らない。text の書式の `0` は本文の字幅（ADR 0139）。
void check_cell(BodyTextLayouts &layouts, const core::EditorSettings &settings)
{
    const app::TextLayoutRequest request{"", settings, narrow, core::DisplayForm::grid};
    const auto cell = layouts.cell(request);
    expect(cell.has_value() && cell.value().width > 0.0F && cell.value().height > 0.0F,
           "the grid cell has a positive width and height");
    const auto again =
        layouts.cell(app::TextLayoutRequest{"ignored", settings, narrow, core::DisplayForm::grid});
    expect(cell.has_value() && again.has_value() && again.value().width == cell.value().width &&
               again.value().height == cell.value().height,
           "the grid cell ignores the text and repeats for the same format");
    const auto zero =
        layouts.locate(app::TextLayoutRequest{"00", settings, narrow, core::DisplayForm::grid},
                       core::DisplayCaret{core::Column{2}});
    expect(cell.has_value() && zero.has_value() && zero.value().x > 0.0F &&
               zero.value().x <= cell.value().width + 0.5F,
           "the cell is the width of the grid font's zero");
    const auto advance = layouts.cell(app::TextLayoutRequest{"", settings, narrow});
    const auto body = layouts.locate(app::TextLayoutRequest{"00", settings, narrow},
                                     core::DisplayCaret{core::Column{2}});
    expect(advance.has_value() && body.has_value() && advance.value().width > 0.0F &&
               body.value().x <= advance.value().width + 0.5F &&
               body.value().x + 0.5F >= advance.value().width,
           "the text cell is the width of the body font's zero");
}

// 巨大な行の窓（ADR 0139 の決定 2・4）。折り返す設定でも窓の layout は 1 画面行で、同じ文字列の
// 折り返す layout とは別に保持し、locate と hit の往復は窓の中で一致する。
void check_giant_window(BodyTextLayouts &layouts, const core::EditorSettings &settings)
{
    const std::string text(9'000, 'm');
    const app::TextLayoutRequest wrapped{text, settings, narrow};
    const app::TextLayoutRequest window{
        text, settings, narrow, core::DisplayForm::text, {}, app::LineWindow{true}};
    const auto folded = layouts.rows(wrapped);
    const auto single = layouts.rows(window);
    expect(folded.has_value() && folded.value() > 1 && single.has_value() && single.value() == 1,
           "the giant line window is one screen row while the same text wraps");
    for (const std::size_t column : {std::size_t{1}, std::size_t{2}, std::size_t{4'500},
                                     std::size_t{8'999}, std::size_t{9'001}})
    {
        check_round_trip(layouts, window, column);
        const auto point = layouts.locate(window, core::DisplayCaret{core::Column{column}});
        expect(point.has_value() && point.value().row == 0,
               "every caret of the window is on its single row");
    }
    expect(layouts.rows(wrapped).value_or(0) == folded.value_or(1),
           "the wrapped layout of the same text is kept apart from the window");
}

// #597: 字体を変えたら本文の字体が変わる。Cascadia Code と Consolas は `0` の送りが違う。
[[nodiscard]] std::wstring family_of_layout(BodyTextLayouts &layouts,
                                            const app::TextLayoutRequest &request)
{
    const auto made = layouts.get(request);
    std::array<wchar_t, 128> name{};
    if (!made || FAILED(made.value()->GetFontFamilyName(0, name.data(), 128)))
    {
        return {};
    }
    return name.data();
}

void check_family_change(BodyTextLayouts &layouts, core::EditorSettings settings)
{
    settings.font_family = core::DisplayText::parse("Cascadia Code").value();
    const auto cascadia = layouts.cell(app::TextLayoutRequest{"", settings, narrow});
    const auto cascadia_name =
        family_of_layout(layouts, app::TextLayoutRequest{"0O", settings, narrow});
    settings.font_family = core::DisplayText::parse("Consolas").value();
    const auto consolas = layouts.cell(app::TextLayoutRequest{"", settings, narrow});
    const auto consolas_name =
        family_of_layout(layouts, app::TextLayoutRequest{"0O", settings, narrow});
    expect(cascadia_name == L"Cascadia Code", "#597 the Cascadia Code format uses Cascadia Code");
    expect(consolas_name == L"Consolas", "#597 the Consolas format uses Consolas");
    expect(cascadia.has_value() && consolas.has_value() &&
               cascadia.value().width != consolas.value().width,
           "#597 changing the family changes the body advance");
}

void check_family_pixels(BodyTextLayouts &layouts, core::EditorSettings settings)
{
    // 描いた画素（DrawTextLayout と保持した字形の 2 経路）も字体で変わる。
    SoftwareTarget target;
    const auto pixels = [&layouts, &target, &settings](const char *family)
    {
        settings.font_family = core::DisplayText::parse(family).value();
        layouts.begin_frame();
        const auto made = layouts.get(app::TextLayoutRequest{"0O iIl1 g", settings, {400, 29, 96}});
        std::pair<SoftwareTarget::Pixels, SoftwareTarget::Pixels> drawn;
        if (!made)
        {
            return drawn;
        }
        drawn.first = target.text_layout(made.value().Get(), D2D1::Point2F(0, 0));
        const auto *entry = layouts.glyphs_of(made.value().Get(), 0, 0, 29);
        if (entry != nullptr && entry->glyphs_ready)
        {
            drawn.second =
                target.glyph_runs(entry->glyphs, D2D1::Point2F(0, 0), D2D1::RectF(0, 0, 400, 29));
        }
        return drawn;
    };
    const auto before = pixels("Cascadia Code");
    const auto after = pixels("Consolas");
    expect(!before.first.empty() && before.first != after.first,
           "#597 the drawn layout changes with the family");
    expect(!before.second.empty() && before.second != after.second,
           "#597 the held glyph runs change with the family");
}

// #597: 格子の文書でも :set guifont= の字体で描く（ADR 0140 の決定 6 は字体を固定している）。
void check_grid_family(BodyTextLayouts &layouts, core::EditorSettings settings)
{
    settings.font_family = core::DisplayText::parse("Cascadia Code").value();
    const app::TextLayoutRequest code{"0O", settings, narrow, core::DisplayForm::grid};
    const auto code_cell = layouts.cell(code);
    const auto code_name = family_of_layout(layouts, code);
    settings.font_family = core::DisplayText::parse("Consolas").value();
    const app::TextLayoutRequest consolas{"0O", settings, narrow, core::DisplayForm::grid};
    const auto consolas_cell = layouts.cell(consolas);
    const auto consolas_name = family_of_layout(layouts, consolas);
    expect(code_name == L"Cascadia Code", "#597 the grid draws with a monospaced guifont");
    expect(consolas_name == L"Consolas", "#597 the grid draws with the guifont family");
    expect(code_cell.has_value() && consolas_cell.has_value() &&
               code_cell.value().width != consolas_cell.value().width,
           "#597 the grid cell follows the guifont family");
}

// #597: 等幅でない字体（Segoe UI）は格子では固定の等幅字体に落ちる（D58）。
void check_grid_proportional(BodyTextLayouts &layouts, core::EditorSettings settings)
{
    settings.font_family = core::DisplayText::parse("Segoe UI").value();
    const auto name = family_of_layout(
        layouts, app::TextLayoutRequest{"0O", settings, narrow, core::DisplayForm::grid});
    expect(name == L"Cascadia Mono" || name == L"Consolas",
           "#597 a proportional guifont falls back to the fixed grid font");
    const auto text = family_of_layout(layouts, app::TextLayoutRequest{"0O", settings, narrow});
    expect(text == L"Segoe UI", "#597 the text form still uses a proportional guifont");
}

// 字体の名前に結果が依るので、3 つの字体が無い機械では測らない（環境依存・QLT-013）。
void check_families(BodyTextLayouts &layouts, const core::EditorSettings &settings)
{
    const auto factory = directwrite_factory();
    if (factory == nullptr || !installed(*factory.Get(), L"Cascadia Code") ||
        !installed(*factory.Get(), L"Consolas") || !installed(*factory.Get(), L"Segoe UI"))
    {
        unmeasured("#597 font family checks need Cascadia Code, Consolas and Segoe UI");
        return;
    }
    check_family_change(layouts, settings);
    check_family_pixels(layouts, settings);
    check_grid_family(layouts, settings);
    check_grid_proportional(layouts, settings);
    nenenib::ui::win32::SystemFontFamilies fonts;
    const auto listed = fonts.list();
    expect(listed.has_value() &&
               std::ranges::find(listed.value(), std::string("Consolas")) != listed.value().end(),
           "#597 the system font families list the installed Consolas by its en-us name");
}
} // namespace

void verify_body_text_layouts()
{
    BodyTextLayouts layouts;
    const auto settings = core::default_editor_settings();
    const auto empty = layouts.rows(app::TextLayoutRequest{"", settings, narrow});
    expect(empty.has_value() && empty.value() == 1, "empty body text has one screen row");
    if (!empty)
    {
        return;
    }
    check_rows(layouts, settings);
    check_unicode(layouts, settings);
    check_resources(layouts, settings);
    check_input_retention(layouts, settings);
    check_wrapped_pixels(layouts, settings);
    check_horizontal_pixels(layouts, settings);
    check_cell(layouts, settings);
    check_giant_window(layouts, settings);
    check_families(layouts, settings);
    const auto invalid =
        layouts.get(app::TextLayoutRequest{std::string_view{"\xff", 1}, settings, narrow});
    expect(!invalid.has_value(), "invalid display text is an explicit layout failure");
    expect(layouts.rows(app::TextLayoutRequest{"ok", settings, narrow}).has_value(),
           "a failed request does not poison a later valid layout");
}
} // namespace nenenib::tests::ui
