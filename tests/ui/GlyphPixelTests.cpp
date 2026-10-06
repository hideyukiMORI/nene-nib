// 字形を保持して描いた画素が DrawTextLayout と 1 画素も違わないこと（ADR 0073）と、
// 字体選択の保持が OS の既定と同じ字体を選ぶこと（ADR 0071）を、
// ソフトウェアの描画先で確かめる。前提は draw_body_text と同じ
// （DPI 96・恒等の変換・整数の原点）。結果はインストール済みのフォントに依る
// （環境依存・QLT-013）ので、無い family は 1 行言って数に入れない。
#include "BodyGlyphCollector.hpp"
#include "DevicePixels.hpp"
#include "DirectWriteFactory.hpp"
#include "FontFallbackCache.hpp"
#include "SoftwareTarget.hpp"
#include "WindowChecks.hpp"

#include <array>
#include <dwrite_3.h>
#include <string>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Format = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using Layout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;

constexpr std::array<const wchar_t *, 3> families{L"Cascadia Code", L"Consolas", L"Yu Gothic UI"};
constexpr std::array<FLOAT, 3> sizes{16.875F, 22.5F, 30.0F};
constexpr std::array<FLOAT, 4> widths{64.0F, 129.0F, 513.0F, 1200.0F};
constexpr FLOAT line_height = 48;
constexpr D2D1_POINT_2F origin{7, 9};

// 異体字セレクタ・ZWJ・双方向の上書きは見えないので、符号位置で足す。
std::wstring mixed_line()
{
    std::wstring text = L"日本語 A\tCafé が 漢";
    text += wchar_t{0xFE00};
    text += L" 👩";
    text += wchar_t{0x200D};
    text += L"💻 אבג العربية 한글 ";
    text += L"ไทย हिन्दी ١٢٣ 制御^A<200b>^M末尾 ";
    return text;
}

std::wstring bidi_line()
{
    std::wstring text = L"العربية אבג 123 ";
    text += wchar_t{0x202E};
    text += L"XYZ";
    text += wchar_t{0x202C};
    text += L" 末尾";
    return text;
}

std::array<std::wstring, 5> sample_texts()
{
    std::wstring long_line = L"000000 ";
    for (int index = 0; index < 90; ++index)
    {
        long_line += L"日本語の長い行と分割表示の文字組みを測定する。";
    }
    return {L"", L"hello\tworld", mixed_line(), long_line, bidi_line()};
}

std::string ascii(const wchar_t *text)
{
    std::string narrow;
    for (const wchar_t *at = text; *at != L'\0'; ++at)
    {
        narrow += static_cast<char>(*at);
    }
    return narrow;
}

Format format_of(IDWriteFactory2 &factory, const wchar_t *family, FLOAT size)
{
    Format format;
    if (FAILED(factory.CreateTextFormat(family, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size,
                                        L"", format.GetAddressOf())) ||
        FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)) ||
        FAILED(format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER)))
    {
        return nullptr;
    }
    return format;
}

// Direct2DRenderer::attach_font_fallback と同じ取り付け。
bool attach(IDWriteFactory2 &factory, const Format &format)
{
    Microsoft::WRL::ComPtr<IDWriteFontFallback> system;
    Microsoft::WRL::ComPtr<IDWriteTextFormat2> version;
    return SUCCEEDED(factory.GetSystemFontFallback(&system)) && SUCCEEDED(format.As(&version)) &&
           SUCCEEDED(version->SetFontFallback(
               Microsoft::WRL::Make<win32::FontFallbackCache>(system).Get()));
}

Layout layout_of(IDWriteFactory2 &factory, IDWriteTextFormat *format, const std::wstring &text,
                 FLOAT width)
{
    Layout layout;
    if (FAILED(factory.CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format,
                                        width, line_height, layout.GetAddressOf())))
    {
        return nullptr;
    }
    return layout;
}

// 字体選択を保持する書式（cached）と OS の既定の書式（plain）の組。
void verify_case(IDWriteFactory2 &factory, SoftwareTarget &target,
                 const std::array<Format, 2> &formats, const std::wstring &text)
{
    for (const auto width : widths)
    {
        Layout cached = layout_of(factory, formats.at(0).Get(), text, width);
        const Layout plain = layout_of(factory, formats.at(1).Get(), text, width);
        const auto collector = Microsoft::WRL::Make<win32::BodyGlyphCollector>(width);
        expect(cached != nullptr && plain != nullptr &&
                   SUCCEEDED(cached->Draw(nullptr, collector.Get(), 0, 0)),
               "the line is laid out and collected");
        if (cached == nullptr || plain == nullptr)
        {
            continue;
        }
        const auto taken = collector->take();
        expect(taken.has_value(), "a line without decorations is not rejected");
        const auto runs = taken.value_or(std::vector<win32::BodyGlyphRun>{});
        const auto expected = target.text_layout(cached.Get(), origin);
        const auto chosen = target.text_layout(plain.Get(), origin);
        cached.Reset();
        const auto drawn = target.glyph_runs(
            runs, origin,
            D2D1::RectF(origin.x, origin.y, origin.x + width, origin.y + line_height));
        expect(!expected.empty() && expected == drawn,
               "retained glyphs draw the same pixels as DrawTextLayout");
        expect(expected == chosen, "the retained font choice draws the same pixels as the OS");
    }
}

void verify_family(IDWriteFactory2 &factory, SoftwareTarget &target, const wchar_t *family)
{
    const auto texts = sample_texts();
    for (const auto size : sizes)
    {
        const std::array<Format, 2> formats{format_of(factory, family, size),
                                            format_of(factory, family, size)};
        expect(formats.at(0) != nullptr && formats.at(1) != nullptr &&
                   attach(factory, formats.at(0)),
               "the formats are made and the retained font choice is attached");
        for (const auto &text : texts)
        {
            verify_case(factory, target, formats, text);
        }
    }
}
} // namespace

void verify_glyph_pixels()
{
    const auto factory = directwrite_factory();
    SoftwareTarget target;
    expect(factory != nullptr && target.ready(), "the software target is made");
    if (factory == nullptr || !target.ready())
    {
        return;
    }
    expect(target.dpi() == static_cast<float>(core::reference_dpi),
           "the software target draws at the renderer's DPI");
    for (const auto *family : families)
    {
        if (!installed(*factory.Get(), family))
        {
            unmeasured(("pixel equality needs the font family " + ascii(family)).c_str());
            continue;
        }
        verify_family(*factory.Get(), target, family);
    }
}
} // namespace nenenib::tests::ui
