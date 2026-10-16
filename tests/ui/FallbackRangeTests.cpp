// Issue #586: 本文の layout（ADR 0142）が、同じ書式で範囲の字体を当てない layout（DirectWrite の
// 代替のまま）と、全 UTF-16 位置の HitTestTextPosition・字形の列・寸法で一致する契約（決定 6 の
// (a)）。当てない側は製品の layout を書式として作り直した layout で、範囲の書式を持たない。
// run の区切りは比べない（DirectWrite は文字種で切り、当てた側は同じ字体がつながる）。run を
// 順に一列に並べた字形ごとに face・字形番号・送り・offset（null は 0 と同じ）・絶対位置を比べ、
// 位置は float の丸め（2 × FLT_EPSILON × 大きい方）を許す。行数と幅は完全一致。
// 字体に結果が依るので、主字体（Cascadia Code）が無い機械では測れない（環境依存・QLT-013）。
#include "BodyGlyphCollector.hpp"
#include "BodyTextLayouts.hpp"
#include "DirectWriteFactory.hpp"
#include "FontFallbackKey.hpp"
#include "PrimaryFontCoverage.hpp"
#include "ScriptedFontFallback.hpp"
#include "WindowChecks.hpp"

#include "EditorSettings.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
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
constexpr std::size_t scattered_units = 2000;
constexpr FLOAT unbounded_width = 1.0e6F;
constexpr FLOAT counted_font_size = 14.0F;

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

// layout の範囲の字体（書式の family と違う family の範囲）の数。読めなければ nullopt。
std::optional<std::size_t> range_fonts(IDWriteTextLayout &layout, UINT32 length)
{
    const auto family = family_of(layout);
    std::size_t count = 0;
    for (UINT32 position = 0; position < length;)
    {
        std::array<wchar_t, name_capacity> name{};
        DWRITE_TEXT_RANGE range{};
        if (FAILED(layout.GetFontFamilyName(position, name.data(), name_capacity, &range)) ||
            range.length == 0)
        {
            return std::nullopt;
        }
        count += family != name.data() ? 1U : 0U;
        position = range.startPosition + range.length;
    }
    return count;
}

// layout の全文に範囲の字体が無い（書式の family のまま）。
bool keeps_format_family(IDWriteTextLayout &layout, UINT32 length)
{
    return range_fonts(layout, length).value_or(1) == 0;
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

// 同じ位置を別の run の区切りで足した float の丸めを許す（折り返さない行の 8,000 px 先で 1 ulp）。
bool close_to(FLOAT left, FLOAT right)
{
    return std::abs(left - right) <= 2 * FLT_EPSILON * std::max(std::abs(left), std::abs(right));
}

bool same_hit(const DWRITE_HIT_TEST_METRICS &left, const DWRITE_HIT_TEST_METRICS &right)
{
    return left.textPosition == right.textPosition && left.length == right.length &&
           close_to(left.left, right.left) && close_to(left.top, right.top) &&
           left.width == right.width && left.height == right.height &&
           left.bidiLevel == right.bidiLevel && left.isText == right.isText &&
           left.isTrimmed == right.isTrimmed;
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
           close_to(points[0], points[2]) && close_to(points[1], points[3]) && same_hit(one, other);
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

// 一列に並べた 1 字形: face・字形番号・絶対位置（origin.x + 送りの和・y）・送り・offset・em・
// bidi・sideways・measuring。
using FlatGlyph = std::tuple<IDWriteFontFace *, UINT16, D2D1_POINT_2F, FLOAT, DWRITE_GLYPH_OFFSET,
                             FLOAT, UINT32, BOOL, DWRITE_MEASURING_MODE>;

std::vector<FlatGlyph> flat(const std::vector<win32::BodyGlyphRun> &runs)
{
    std::vector<FlatGlyph> glyphs;
    for (const auto &run : runs)
    {
        FLOAT x = run.origin.x;
        for (std::size_t index = 0; index < run.indices.size(); ++index)
        {
            // glyphOffsets が null の run は offsets が空で、0 の配列と同じ。
            const auto offset =
                run.offsets.empty() ? DWRITE_GLYPH_OFFSET{0, 0} : run.offsets.at(index);
            glyphs.emplace_back(run.face.Get(), run.indices.at(index),
                                D2D1_POINT_2F{x, run.origin.y}, run.advances.at(index), offset,
                                run.em, run.bidi, run.sideways, run.measuring);
            x += run.advances.at(index);
        }
    }
    return glyphs;
}

bool same_glyph(const FlatGlyph &left, const FlatGlyph &right)
{
    const auto &[face, index, point, advance, offset, em, bidi, sideways, measuring] = left;
    return same_face(face, std::get<0>(right)) && index == std::get<1>(right) &&
           close_to(point.x, std::get<2>(right).x) && close_to(point.y, std::get<2>(right).y) &&
           advance == std::get<3>(right) &&
           offset.advanceOffset == std::get<4>(right).advanceOffset &&
           offset.ascenderOffset == std::get<4>(right).ascenderOffset && em == std::get<5>(right) &&
           bidi == std::get<6>(right) && sideways == std::get<7>(right) &&
           measuring == std::get<8>(right);
}

// 横に画面外の run を除かない collector で集める。除外（ADR 0073）は run の単位なので、
// run がつながる当てた側では効き方が変わる（#586 の残るリスク・工程 2）。ここは字形の中身を比べる。
Glyphs glyphs_of(IDWriteTextLayout &layout)
{
    const auto collector =
        Microsoft::WRL::Make<win32::BodyGlyphCollector>(std::numeric_limits<FLOAT>::max());
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
    if (!one.has_value() || !other.has_value())
    {
        return false;
    }
    const auto first = flat(one.value());
    const auto second = flat(other.value());
    return std::equal(first.begin(), first.end(), second.begin(), second.end(), same_glyph);
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
    expect(glyphs,
           "range fonts keep the glyph sequence: faces, indices, advances, offsets and positions");
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
// 製品の layout は主字体に無い文字の区間に範囲の字体を持ち、1 行の区間は上限で止まる（決定 4）。
void check_assigned(BodyTextLayouts &layouts)
{
    auto settings = core::default_editor_settings();
    settings.wrapping = core::LineWrapping::no_wrap;
    constexpr core::TextViewGeometry wide{900, 29, 96};
    const auto japanese = layouts.get(app::TextLayoutRequest{"abc 日本語 def", settings, wide});
    expect(japanese.has_value() && range_fonts(*japanese.value().Get(), 11).value_or(0) == 1,
           "the run missing from the primary font carries the system's range font");
    const auto ascii = layouts.get(app::TextLayoutRequest{"plain ASCII only", settings, wide});
    expect(ascii.has_value() && range_fonts(*ascii.value().Get(), 16).value_or(1) == 0,
           "a line the primary font covers carries no range font");
    std::string many;
    for (std::size_t index = 0; index < win32::fallback_range_limit + 1000; ++index)
    {
        many += "あ ";
    }
    const auto limited = layouts.get(app::TextLayoutRequest{many, settings, wide});
    const auto length = static_cast<UINT32>(core::to_utf16(many).value_or(std::wstring{}).size());
    expect(limited.has_value() && range_fonts(*limited.value().Get(), length).value_or(0) ==
                                      win32::fallback_range_limit,
           "a line beyond the limit carries exactly the limit of range fonts");
}
// 1 つの区切りが保持の鍵の上限を超える行（約 2,000 単位）。数字と空白（Common）の間に主字体に無い
// 漢字が点在し、AnalyzeScript は全体を Han の 1 つの区切りにする。
std::wstring scattered_line()
{
    std::string line;
    while (line.size() < scattered_units)
    {
        line += "0123456789 漢";
    }
    return core::to_utf16(line).value_or(std::wstring{});
}

// 字体選択に渡す長さを数える替え玉（OS の字体選択へ委ねる）を付けた、折り返さない本文の書式。
Microsoft::WRL::ComPtr<IDWriteTextFormat> counted_format(IDWriteFactory2 &factory,
                                                         ScriptedFontFallback &counter)
{
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat1> version;
    Microsoft::WRL::ComPtr<IDWriteFontFallback> system;
    if (FAILED(factory.CreateTextFormat(L"Cascadia Code", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                        counted_font_size, L"", &format)) ||
        FAILED(format.As(&version)) || FAILED(factory.GetSystemFontFallback(&system)) ||
        FAILED(version->SetFontFallback(&counter)) ||
        FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)))
    {
        return nullptr;
    }
    counter.delegate_to(system);
    return format;
}

// 長い区切りは保持の鍵の上限までの片で字体選択へ渡し（決定 3）、当てた字形の列は当てない layout
// と一致する。
void check_long_segment(IDWriteFactory2 &factory)
{
    const auto counter = Microsoft::WRL::Make<ScriptedFontFallback>();
    const auto wide = scattered_line();
    const auto length = static_cast<UINT32>(wide.size());
    const auto format = counted_format(factory, *counter.Get());
    Layout applied;
    if (format == nullptr ||
        FAILED(factory.CreateTextLayout(wide.c_str(), length, format.Get(), unbounded_width,
                                        unbounded_width, applied.GetAddressOf())))
    {
        expect(false, "the long segment has a counted layout");
        return;
    }
    auto coverage = win32::PrimaryFontCoverage::of(factory, *format.Get());
    counter->forget_longest();
    const bool assigned =
        coverage.has_value() && SUCCEEDED(coverage.value().assign(*applied.Get(), wide));
    const auto longest = counter->longest();
    std::printf("fallback ranges: the long segment passed at most %u UTF-16 units\n", longest);
    expect(assigned && range_fonts(*applied.Get(), length).value_or(0) > 0,
           "the long segment carries range fonts");
    expect(longest > 0 && longest <= win32::fallback_key_text_limit,
           "the font fallback receives at most the cache key limit of a long segment at once");
    const auto plain = plain_of(factory, *applied.Get(), wide);
    expect(plain != nullptr && same_glyphs(*applied.Get(), *plain.Get()) &&
               !first_hit_difference(*applied.Get(), *plain.Get(), length).has_value(),
           "the long segment keeps the glyph sequence and every hit position");
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
    check_assigned(layouts);
    check_long_segment(*factory.Get());
}
} // namespace nenenib::tests::ui
