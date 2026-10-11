#include "BodyTextLayouts.hpp"
#include "WindowChecks.hpp"

#include "EditorSettings.hpp"
#include "Utf8.hpp"

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
    const auto invalid =
        layouts.get(app::TextLayoutRequest{std::string_view{"\xff", 1}, settings, narrow});
    expect(!invalid.has_value(), "invalid display text is an explicit layout failure");
    expect(layouts.rows(app::TextLayoutRequest{"ok", settings, narrow}).has_value(),
           "a failed request does not poison a later valid layout");
}
} // namespace nenenib::tests::ui
