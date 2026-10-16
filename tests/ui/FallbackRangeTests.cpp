// Issue #586: 本文の layout（ADR 0142）が、同じ書式で範囲の字体を当てない layout（DirectWrite の
// 代替のまま）と、全 UTF-16 位置の HitTestTextPosition・字形の収集・寸法で一致する契約（決定 6 の
// (a)）。当てない側は製品の layout を書式として作り直した layout で、範囲の書式を持たない。
// 字体に結果が依るので、主字体（Cascadia Code）が無い機械では測れない（環境依存・QLT-013）。
#include "BodyGlyphCollector.hpp"
#include "BodyTextLayouts.hpp"
#include "DirectWriteFactory.hpp"
#include "WindowChecks.hpp"

#include "EditorSettings.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
namespace win32 = nenenib::ui::win32;
using Layout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;
using Glyphs = std::optional<std::vector<win32::BodyGlyphRun>>;
using win32::BodyTextLayouts;

constexpr std::size_t name_capacity = 128;
constexpr std::size_t bench_repeats = 40;

// 見本（ADR 0142 の決定 6）。日本語・漢字かな混在・ASCII 混在・絵文字（ZWJ の家族・国旗・VS16）・
// 結合文字（e + U+0301・濁点 U+3099・半濁点 U+309A）・ハングル・Tab 入り・双方向・空行。
constexpr std::array<std::string_view, 9> samples{
    "日本語の長い行を折り返してカーソル位置を確かめる。漢字とかなとカタカナ、句読点も混ぜる。",
    "漢字かな混じり文：東京都の天気は晴れ（最高気温２５度）です。「引用」と…省略。",
    "Japanese 日本語 mixed with ASCII 123 and かな. End.",
    "family \U0001F468‍\U0001F469‍\U0001F467‍\U0001F466 flags "
    "\U0001F1EF\U0001F1F5\U0001F1FA\U0001F1F8 heart ❤️ end",
    "é café が ぱ ガギ ä́",
    "한국어 문장과 日本語 混在 한글",
    "日本\t語\tend\t한글\tx",
    "abc العربية def 日本 שלום 123 עברית end",
    "",
};

// 長い行のベンチの文書と同じ形（`000000 ` と日本語の文の繰り返し・約 2,000 code point）。
std::string bench_line()
{
    std::string line = "000000 ";
    for (std::size_t index = 0; index < bench_repeats; ++index)
    {
        line += "日本語の長い行で一打鍵の時間を測るための文章です。";
    }
    return line;
}

std::wstring family_of(IDWriteTextFormat &format)
{
    std::array<wchar_t, name_capacity> name{};
    return SUCCEEDED(format.GetFontFamilyName(name.data(), name_capacity)) ? name.data()
                                                                           : std::wstring{};
}

// 製品の layout を書式として、範囲の書式を持たない layout を作る（当てない側）。
Layout plain_of(IDWriteFactory2 &factory, IDWriteTextLayout &applied, const std::wstring &wide)
{
    Layout plain;
    if (FAILED(factory.CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()), &applied,
                                        applied.GetMaxWidth(), applied.GetMaxHeight(),
                                        plain.GetAddressOf())) ||
        FAILED(plain->SetWordWrapping(applied.GetWordWrapping())))
    {
        return nullptr;
    }
    return plain;
}

// layout の全文に範囲の字体が無い（書式の family のまま）。
bool keeps_format_family(IDWriteTextLayout &layout, UINT32 length)
{
    const auto family = family_of(layout);
    for (UINT32 position = 0; position < length;)
    {
        std::array<wchar_t, name_capacity> name{};
        DWRITE_TEXT_RANGE range{};
        if (FAILED(layout.GetFontFamilyName(position, name.data(), name_capacity, &range)) ||
            family != name.data() || range.length == 0)
        {
            return false;
        }
        position = range.startPosition + range.length;
    }
    return true;
}

bool same_fallback(IDWriteTextLayout &left, IDWriteTextLayout &right)
{
    Microsoft::WRL::ComPtr<IDWriteTextFormat1> first;
    Microsoft::WRL::ComPtr<IDWriteTextFormat1> second;
    Microsoft::WRL::ComPtr<IDWriteFontFallback> one;
    Microsoft::WRL::ComPtr<IDWriteFontFallback> other;
    return SUCCEEDED(Layout(&left).As(&first)) && SUCCEEDED(Layout(&right).As(&second)) &&
           SUCCEEDED(first->GetFontFallback(&one)) && SUCCEEDED(second->GetFontFallback(&other)) &&
           one != nullptr && one == other;
}

bool same_hit(const DWRITE_HIT_TEST_METRICS &left, const DWRITE_HIT_TEST_METRICS &right)
{
    return left.textPosition == right.textPosition && left.length == right.length &&
           left.left == right.left && left.top == right.top && left.width == right.width &&
           left.height == right.height && left.bidiLevel == right.bidiLevel &&
           left.isText == right.isText && left.isTrimmed == right.isTrimmed;
}

// 1 つの UTF-16 位置の片側（leading / trailing）で、点と hit の寸法が一致する。
bool same_position(IDWriteTextLayout &left, IDWriteTextLayout &right, UINT32 position,
                   BOOL trailing)
{
    std::array<FLOAT, 4> points{};
    DWRITE_HIT_TEST_METRICS one{};
    DWRITE_HIT_TEST_METRICS other{};
    return SUCCEEDED(left.HitTestTextPosition(position, trailing, &points[0], &points[1], &one)) &&
           SUCCEEDED(
               right.HitTestTextPosition(position, trailing, &points[2], &points[3], &other)) &&
           points[0] == points[2] && points[1] == points[3] && same_hit(one, other);
}

// 最初に違った UTF-16 位置。全位置・leading と trailing で一致すれば nullopt。
std::optional<UINT32> first_hit_difference(IDWriteTextLayout &left, IDWriteTextLayout &right,
                                           UINT32 length)
{
    for (UINT32 position = 0; position <= length; ++position)
    {
        if (!same_position(left, right, position, FALSE) ||
            !same_position(left, right, position, TRUE))
        {
            return position;
        }
    }
    return std::nullopt;
}

bool same_text_metrics(IDWriteTextLayout &left, IDWriteTextLayout &right)
{
    DWRITE_TEXT_METRICS one{};
    DWRITE_TEXT_METRICS other{};
    return SUCCEEDED(left.GetMetrics(&one)) && SUCCEEDED(right.GetMetrics(&other)) &&
           one.left == other.left && one.top == other.top && one.width == other.width &&
           one.widthIncludingTrailingWhitespace == other.widthIncludingTrailingWhitespace &&
           one.height == other.height && one.lineCount == other.lineCount &&
           one.maxBidiReorderingDepth == other.maxBidiReorderingDepth;
}

std::vector<DWRITE_LINE_METRICS> lines_of(IDWriteTextLayout &layout)
{
    UINT32 count = 0;
    static_cast<void>(layout.GetLineMetrics(nullptr, 0, &count));
    std::vector<DWRITE_LINE_METRICS> lines(count);
    if (FAILED(layout.GetLineMetrics(lines.data(), count, &count)))
    {
        lines.clear();
    }
    return lines;
}

bool same_lines(IDWriteTextLayout &left, IDWriteTextLayout &right)
{
    const auto one = lines_of(left);
    const auto other = lines_of(right);
    return one.size() == other.size() &&
           std::equal(one.begin(), one.end(), other.begin(),
                      [](const DWRITE_LINE_METRICS &first, const DWRITE_LINE_METRICS &second)
                      {
                          return first.length == second.length &&
                                 first.trailingWhitespaceLength ==
                                     second.trailingWhitespaceLength &&
                                 first.newlineLength == second.newlineLength &&
                                 first.height == second.height &&
                                 first.baseline == second.baseline &&
                                 first.isTrimmed == second.isTrimmed;
                      });
}

std::vector<std::byte> file_key_of(IDWriteFontFace &face)
{
    UINT32 count = 1;
    Microsoft::WRL::ComPtr<IDWriteFontFile> file;
    const void *key = nullptr;
    UINT32 size = 0;
    if (FAILED(face.GetFiles(&count, file.GetAddressOf())) || file == nullptr ||
        FAILED(file->GetReferenceKey(&key, &size)) || key == nullptr)
    {
        return {};
    }
    const auto *bytes = static_cast<const std::byte *>(key);
    return {bytes, bytes + size};
}

// face の等価は同じ字体ファイル・同じ face index・同じ simulation（同じ物を指す別の COM
// でもよい）。
bool same_face(IDWriteFontFace *left, IDWriteFontFace *right)
{
    if (left == right)
    {
        return true;
    }
    return left != nullptr && right != nullptr && left->GetIndex() == right->GetIndex() &&
           left->GetSimulations() == right->GetSimulations() && !file_key_of(*left).empty() &&
           file_key_of(*left) == file_key_of(*right);
}

bool same_offsets(const std::vector<DWRITE_GLYPH_OFFSET> &left,
                  const std::vector<DWRITE_GLYPH_OFFSET> &right)
{
    return std::equal(left.begin(), left.end(), right.begin(), right.end(),
                      [](const DWRITE_GLYPH_OFFSET &one, const DWRITE_GLYPH_OFFSET &other)
                      {
                          return one.advanceOffset == other.advanceOffset &&
                                 one.ascenderOffset == other.ascenderOffset;
                      });
}

bool same_run(const win32::BodyGlyphRun &left, const win32::BodyGlyphRun &right)
{
    return same_face(left.face.Get(), right.face.Get()) && left.em == right.em &&
           left.sideways == right.sideways && left.bidi == right.bidi &&
           left.origin.x == right.origin.x && left.origin.y == right.origin.y &&
           left.measuring == right.measuring && left.indices == right.indices &&
           left.advances == right.advances && same_offsets(left.offsets, right.offsets) &&
           left.text_position == right.text_position && left.clusters == right.clusters &&
           left.advance == right.advance;
}

Glyphs glyphs_of(IDWriteTextLayout &layout)
{
    const auto collector = Microsoft::WRL::Make<win32::BodyGlyphCollector>(layout.GetMaxWidth());
    if (collector == nullptr)
    {
        return std::nullopt;
    }
    static_cast<void>(layout.Draw(nullptr, collector.Get(), 0, 0));
    return collector->take();
}

bool same_glyphs(IDWriteTextLayout &left, IDWriteTextLayout &right)
{
    const auto one = glyphs_of(left);
    const auto other = glyphs_of(right);
    return one.has_value() && other.has_value() &&
           std::equal(one.value().begin(), one.value().end(), other.value().begin(),
                      other.value().end(), same_run);
}

void report(std::size_t sample, const char *what, UINT32 position)
{
    std::printf("fallback ranges: sample %zu differs in %s at UTF-16 %u\n", sample, what, position);
}

// 1 つの見本・1 つの幾何で、当てた layout（製品）と当てない layout を比べる。
void check_sample(BodyTextLayouts &layouts, const app::TextLayoutRequest &request,
                  std::size_t sample)
{
    auto *factory = directwrite_factory().Get();
    const auto applied = layouts.get(request);
    const auto wide = core::to_utf16(request.text);
    if (!applied || !wide || factory == nullptr)
    {
        expect(false, "the sample has a product layout");
        return;
    }
    const auto length = static_cast<UINT32>(wide.value().size());
    const auto plain = plain_of(*factory, *applied.value().Get(), wide.value());
    expect(plain != nullptr && keeps_format_family(*plain.Get(), length) &&
               same_fallback(*plain.Get(), *applied.value().Get()),
           "the plain layout keeps the format and its fallback without range fonts");
    if (plain == nullptr)
    {
        return;
    }
    const auto hit = first_hit_difference(*applied.value().Get(), *plain.Get(), length);
    if (hit.has_value())
    {
        report(sample, "HitTestTextPosition", hit.value());
    }
    expect(!hit.has_value(), "every UTF-16 position hits the same place with range fonts");
    const bool metrics = same_text_metrics(*applied.value().Get(), *plain.Get()) &&
                         same_lines(*applied.value().Get(), *plain.Get());
    if (!metrics)
    {
        report(sample, "GetMetrics / GetLineMetrics", 0);
    }
    expect(metrics, "range fonts keep the line count, widths and line metrics");
    const bool glyphs = same_glyphs(*applied.value().Get(), *plain.Get());
    if (!glyphs)
    {
        report(sample, "collected glyphs", 0);
    }
    expect(glyphs, "range fonts keep the collected faces, indices, advances, offsets and origins");
}

// 見本を 4 つの幾何で: 狭い折り返し・広い折り返し・折り返さない設定・巨大な行の窓（ADR 0139）。
void check_geometries(BodyTextLayouts &layouts, std::string_view text, std::size_t sample)
{
    const auto settings = core::default_editor_settings();
    auto no_wrap = settings;
    no_wrap.wrapping = core::LineWrapping::no_wrap;
    constexpr core::TextViewGeometry narrow{72, 29, 96};
    constexpr core::TextViewGeometry wide{900, 29, 144};
    check_sample(layouts, app::TextLayoutRequest{text, settings, narrow}, sample);
    check_sample(layouts, app::TextLayoutRequest{text, settings, wide}, sample);
    check_sample(layouts, app::TextLayoutRequest{text, no_wrap, wide}, sample);
    check_sample(layouts,
                 app::TextLayoutRequest{
                     text, settings, wide, core::DisplayForm::text, {}, app::LineWindow{true}},
                 sample);
}
} // namespace

void verify_fallback_ranges()
{
    const auto factory = directwrite_factory();
    if (factory == nullptr || !installed(*factory.Get(), L"Cascadia Code"))
    {
        unmeasured("fallback ranges need the default body font Cascadia Code");
        return;
    }
    BodyTextLayouts layouts;
    for (std::size_t sample = 0; sample < samples.size(); ++sample)
    {
        check_geometries(layouts, samples.at(sample), sample);
    }
    const auto bench = bench_line();
    check_geometries(layouts, bench, samples.size());
}
} // namespace nenenib::tests::ui
