#include "DirectWriteFactory.hpp"
#include "GutterTextLayouts.hpp"
#include "SoftwareTarget.hpp"
#include "WindowChecks.hpp"

#include <array>
#include <string>

namespace nenenib::tests::ui
{
namespace
{
using Format = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using Layout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;
using nenenib::ui::win32::GutterTextLayouts;
constexpr core::LayoutRect area{7, 9, 136, 57};

Layout make_layout(IDWriteFactory2 &factory, IDWriteTextFormat *format, std::wstring_view text,
                   const core::LayoutRect &bounds)
{
    Layout layout;
    const auto made =
        factory.CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), format,
                                 static_cast<float>(core::width_of(bounds)),
                                 static_cast<float>(core::height_of(bounds)), &layout);
    expect(SUCCEEDED(made), "the contract layout is created");
    return layout;
}

bool has_layout(GutterTextLayouts &cache, std::string_view text, const core::LayoutRect &bounds,
                const Layout &expected)
{
    const auto found = cache.lookup(text, bounds);
    return found.has_value() && found.value().Get() == expected.Get();
}

void verify_keys(const Layout &first, const Layout &second)
{
    GutterTextLayouts cache;
    cache.begin();
    expect(!cache.lookup("1", area).has_value(), "an absent key is optional absence");
    cache.retain("1", area, nullptr);
    expect(!cache.lookup("1", area).has_value(), "a failed or null creation is not retained");
    std::string number = "1";
    cache.retain(number, area, first);
    number = "different";
    expect(has_layout(cache, "1", area, first), "the key owns the whole text");
    cache.retain("1", area, second);
    expect(has_layout(cache, "1", area, first), "same-frame duplicate keeps the original resource");
    expect(!cache.lookup("2", area).has_value(), "different text misses");
    expect(!cache.lookup("1", {7, 9, 137, 57}).has_value(), "different width misses");
    expect(!cache.lookup("1", {7, 9, 136, 58}).has_value(), "different height misses");
    expect(has_layout(cache, "1", {40, 20, 169, 68}, first),
           "origin is not a key and color is not stored");
    cache.retain("2", area, second);
    cache.end();
    cache.begin();
    expect(has_layout(cache, "1", area, first), "previous frame moves to current");
    cache.end();
    expect(!cache.lookup("2", area).has_value(), "unused previous-frame resources are discarded");
    cache.begin();
    cache.retain("2", area, second);
    cache.clear();
    expect(!cache.lookup("1", area).has_value() && !cache.lookup("2", area).has_value(),
           "font or DPI invalidation clears both columns");
    cache.retain("1", area, second);
    expect(has_layout(cache, "1", area, second),
           "a failed creation or invalidation can be retried");
}

void verify_pixels(IDWriteFactory2 &factory, IDWriteTextFormat *format)
{
    SoftwareTarget target;
    if (!target.ready())
    {
        unmeasured("gutter/title pixels: software target unavailable");
        return;
    }
    constexpr std::array<std::wstring_view, 4> numbers{L"1", L"99", L"123456", L"99999999999999"};
    expect(SUCCEEDED(format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING)),
           "gutter uses the one trailing alignment");
    for (const auto text : numbers)
    {
        const auto layout = make_layout(factory, format, text, area);
        if (!layout)
        {
            return;
        }
        const auto expected = target.text_w(text, format, area);
        expect(!expected.empty() && expected == target.plain_layout(layout.Get(), area),
               "right-aligned gutter layouts match DrawTextW byte for byte");
    }
    expect(SUCCEEDED(format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING)),
           "palette title uses leading alignment");
    constexpr std::array<std::wstring_view, 4> titles{
        L"設定", L"á café 日本😀", L"العربية אבג 123",
        L"長い名前が欄からはみ出しても同じ位置に描く"};
    for (const auto text : titles)
    {
        const auto layout = make_layout(factory, format, text, area);
        if (!layout)
        {
            return;
        }
        DWRITE_TEXT_METRICS metrics{};
        expect(SUCCEEDED(layout->GetMetrics(&metrics)), "the drawn title supplies its own metrics");
        const auto expected = target.text_w(text, format, area);
        expect(!expected.empty() && expected == target.plain_layout(layout.Get(), area),
               "the measured palette title matches DrawTextW byte for byte");
    }
}
} // namespace

void verify_gutter_layouts()
{
    const auto factory = directwrite_factory();
    if (!factory)
    {
        unmeasured("gutter/title contracts: DirectWrite unavailable");
        return;
    }
    Format format;
    const auto made = factory->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                                DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL, 18.0F, L"", &format);
    expect(SUCCEEDED(made), "the gutter/title contract format is created");
    if (!format)
    {
        return;
    }
    expect(SUCCEEDED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)),
           "one line does not wrap");
    expect(SUCCEEDED(format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER)),
           "the row's vertical alignment matches the renderer");
    const auto first = make_layout(*factory.Get(), format.Get(), L"1", area);
    const auto second = make_layout(*factory.Get(), format.Get(), L"2", area);
    if (!first || !second)
    {
        return;
    }
    verify_keys(first, second);
    if (!installed(*factory.Get(), L"Consolas"))
    {
        unmeasured("gutter/title pixels: Consolas unavailable");
        return;
    }
    verify_pixels(*factory.Get(), format.Get());
}
} // namespace nenenib::tests::ui
