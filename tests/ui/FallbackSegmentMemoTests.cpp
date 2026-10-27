// Issue #884: 字体選択の区切りの片の文字列の覚え（ADR 0142 の追補）。同じ片の文字列を 2 回通すと
// 2 回目は OS の字体選択（替え玉）を呼ばず、違う文字列は呼び、null の答えも覚え、失敗は覚えず、
// 4096 件を超えると捨てて呼び直す。覚えの型の検査は字体に依らない。Issue #889: 覚えの字体は
// (family 名・weight・style・stretch) の組で正規化し、同じ組の別の物は同じ行（同じ ComPtr）になり、
// family 名を読むのは新しい物のときだけで、隣り合う片は 1 つの範囲にまとまる。PrimaryFontCoverage
// を通す検査は主字体（Cascadia Code）に依るので、無い機械では測れない（環境依存・QLT-013）。
#include "DirectWriteFactory.hpp"
#include "FallbackSegmentMemo.hpp"
#include "FontFallbackKey.hpp"
#include "ForwardingFont.hpp"
#include "PrimaryFontCoverage.hpp"
#include "ScriptedFontFallback.hpp"
#include "WindowChecks.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Format = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using Layout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;
using Font = Microsoft::WRL::ComPtr<IDWriteFont>;

constexpr FLOAT memo_font_size = 14.0F;
constexpr FLOAT memo_unbounded = 1.0e6F;
constexpr wchar_t han_first = 0x4E00;
constexpr std::size_t name_capacity = 128;
constexpr FLOAT remembered_scale = 1.5F;
constexpr UINT32 remembered_mapped = 3;
constexpr std::size_t long_segment = 600;

// 番号ごとに違う 1 文字の漢字（U+4E00 から）。
std::wstring han(std::size_t index)
{
    return std::wstring(1, static_cast<wchar_t>(han_first + index));
}

// 覚えの型だけの検査（字体に依らない）。
void check_memo_type()
{
    win32::FallbackSegmentMemo memo;
    memo.remember(L"日本", std::nullopt, remembered_scale, remembered_mapped);
    const auto found = memo.find(L"日本");
    expect(found.has_value() && !memo.font(found.value()).has_value() &&
               memo.scale(found.value()) == remembered_scale &&
               memo.mapped(found.value()) == remembered_mapped,
           "the segment memo keeps a null font, the scale and the mapped length of a text");
    expect(!memo.find(L"日").has_value() && !memo.find(L"日本語").has_value(),
           "the segment memo answers only the same text");
    memo.remember(L"", std::nullopt, 1, 1);
    memo.remember(std::wstring(win32::fallback_key_text_limit + 1, L'x'), std::nullopt, 1, 1);
    expect(memo.size() == 1, "the segment memo skips an empty text and one beyond the key limit");
    win32::FallbackSegmentMemo full;
    for (std::size_t index = 0; index < win32::fallback_segment_memo_limit; ++index)
    {
        full.remember(han(index), std::nullopt, 1, 1);
    }
    const bool held = full.size() == win32::fallback_segment_memo_limit &&
                      full.find(han(0)).has_value() &&
                      full.find(han(win32::fallback_segment_memo_limit - 1)).has_value();
    full.remember(han(win32::fallback_segment_memo_limit), std::nullopt, 1, 1);
    expect(held && full.size() == 1 && !full.find(han(0)).has_value() &&
               full.find(han(win32::fallback_segment_memo_limit)).has_value(),
           "the segment memo holds its limit and forgets everything beyond it");
}

// 替え玉の字体選択を付けた、折り返さない Cascadia Code の書式。
Format scripted_format(IDWriteFactory2 &factory, ScriptedFontFallback &fallback)
{
    Format format;
    Microsoft::WRL::ComPtr<IDWriteTextFormat1> version;
    if (FAILED(factory.CreateTextFormat(L"Cascadia Code", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                        memo_font_size, L"", &format)) ||
        FAILED(format.As(&version)) || FAILED(version->SetFontFallback(&fallback)) ||
        FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)))
    {
        return nullptr;
    }
    return format;
}

// 書式から wide の layout を作る。
Layout layout_of(IDWriteFactory2 &factory, IDWriteTextFormat &format, const std::wstring &wide)
{
    Layout layout;
    if (FAILED(factory.CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()), &format,
                                        memo_unbounded, memo_unbounded, layout.GetAddressOf())))
    {
        return nullptr;
    }
    return layout;
}

// coverage で wide を layout に当てる間に替え玉が呼ばれた回数。当てられなければ nullopt。
std::optional<UINT32> assigned_calls(win32::PrimaryFontCoverage &coverage,
                                     const ScriptedFontFallback &fallback, const Layout &layout,
                                     const std::wstring &wide)
{
    const auto before = fallback.calls();
    if (layout == nullptr || FAILED(coverage.assign(*layout.Get(), wide)))
    {
        return std::nullopt;
    }
    return fallback.calls() - before;
}

// 書式から作った新しい layout に wide を当てる間に替え玉が呼ばれた回数。
std::optional<UINT32> calls_for(IDWriteTextFormat &format, win32::PrimaryFontCoverage &coverage,
                                const ScriptedFontFallback &fallback, const std::wstring &wide)
{
    const auto factory = directwrite_factory();
    if (factory == nullptr)
    {
        return std::nullopt;
    }
    return assigned_calls(coverage, fallback, layout_of(*factory.Get(), format, wide), wide);
}

// layout の position の family 名。読めなければ空。
std::wstring family_at(IDWriteTextLayout &layout, UINT32 position)
{
    std::array<wchar_t, name_capacity> name{};
    return SUCCEEDED(layout.GetFontFamilyName(position, name.data(), name_capacity))
               ? name.data()
               : std::wstring{};
}

// 替え玉だけの答え（字体は null・S_OK）: 同じ文字列の 2 回目は呼ばず、違う文字列は呼ぶ。
void check_null_answers(IDWriteFactory2 &factory)
{
    const auto fallback = Microsoft::WRL::Make<ScriptedFontFallback>();
    const auto format = scripted_format(factory, *fallback.Get());
    auto coverage =
        format != nullptr ? win32::PrimaryFontCoverage::of(factory, *format.Get()) : std::nullopt;
    if (!coverage.has_value())
    {
        expect(false, "the scripted format has a primary font coverage");
        return;
    }
    auto &covered = coverage.value();
    const auto first = calls_for(*format.Get(), covered, *fallback.Get(), L"abc 日本語 def");
    const auto second = calls_for(*format.Get(), covered, *fallback.Get(), L"abc 日本語 def");
    const auto other = calls_for(*format.Get(), covered, *fallback.Get(), L"abc 中国語 def");
    expect(first.value_or(0) > 0 && second.value_or(1) == 0,
           "the second pass of the same segment text skips the font fallback, null answers too");
    expect(other.value_or(0) > 0, "a different segment text calls the font fallback");
}

// 失敗した答えは覚えず、次も呼ぶ。
void check_failures(IDWriteFactory2 &factory)
{
    const auto fallback = Microsoft::WRL::Make<ScriptedFontFallback>();
    fallback->set_result(E_FAIL);
    const auto format = scripted_format(factory, *fallback.Get());
    auto coverage =
        format != nullptr ? win32::PrimaryFontCoverage::of(factory, *format.Get()) : std::nullopt;
    if (!coverage.has_value())
    {
        expect(false, "the failing format has a primary font coverage");
        return;
    }
    const auto first =
        calls_for(*format.Get(), coverage.value(), *fallback.Get(), L"abc 日本語 def");
    const auto second =
        calls_for(*format.Get(), coverage.value(), *fallback.Get(), L"abc 日本語 def");
    expect(first.value_or(0) > 0 && second.value_or(0) == first.value_or(0),
           "a failed font fallback is not remembered and is called again");
}

// OS の字体選択へ委ねた答え（字体あり）も覚え、2 回目の layout は同じ字体を持つ。
void check_system_answers(IDWriteFactory2 &factory)
{
    const auto fallback = Microsoft::WRL::Make<ScriptedFontFallback>();
    Microsoft::WRL::ComPtr<IDWriteFontFallback> system;
    const auto format = scripted_format(factory, *fallback.Get());
    auto coverage =
        format != nullptr ? win32::PrimaryFontCoverage::of(factory, *format.Get()) : std::nullopt;
    if (!coverage.has_value() || FAILED(factory.GetSystemFontFallback(&system)))
    {
        expect(false, "the delegating format has a primary font coverage");
        return;
    }
    fallback->delegate_to(system);
    const std::wstring wide = L"abc 日本語 def";
    const auto one = layout_of(factory, *format.Get(), wide);
    const auto two = layout_of(factory, *format.Get(), wide);
    const auto first = assigned_calls(coverage.value(), *fallback.Get(), one, wide);
    const auto second = assigned_calls(coverage.value(), *fallback.Get(), two, wide);
    constexpr UINT32 japanese = 4;
    const bool same = one != nullptr && two != nullptr &&
                      family_at(*one.Get(), japanese) != L"Cascadia Code" &&
                      !family_at(*one.Get(), japanese).empty() &&
                      family_at(*one.Get(), japanese) == family_at(*two.Get(), japanese);
    expect(first.value_or(0) > 0 && second.value_or(1) == 0 && same,
           "a remembered system answer skips the font fallback and assigns the same range font");
}

// 4096 件を超える違う区切りを通すと捨てて覚え直し、捨てた文字列は呼び直す。
void check_overflow(IDWriteFactory2 &factory)
{
    const auto fallback = Microsoft::WRL::Make<ScriptedFontFallback>();
    const auto format = scripted_format(factory, *fallback.Get());
    auto coverage =
        format != nullptr ? win32::PrimaryFontCoverage::of(factory, *format.Get()) : std::nullopt;
    if (!coverage.has_value())
    {
        expect(false, "the overflowing format has a primary font coverage");
        return;
    }
    // Latin の a と漢字 1 文字が交互に並び、漢字の区切りは全部違う文字列。
    std::wstring many;
    for (std::size_t index = 0; index <= win32::fallback_segment_memo_limit; ++index)
    {
        many += L"a" + han(index);
    }
    const auto line = calls_for(*format.Get(), coverage.value(), *fallback.Get(), many);
    const auto last = calls_for(*format.Get(), coverage.value(), *fallback.Get(),
                                L"a" + han(win32::fallback_segment_memo_limit));
    const auto dropped = calls_for(*format.Get(), coverage.value(), *fallback.Get(), L"a" + han(0));
    expect(line.value_or(0) == win32::fallback_segment_memo_limit + 1 && last.value_or(1) == 0 &&
               dropped.value_or(0) == 1,
           "beyond the memo limit the memo forgets and the forgotten text calls again");
}

// 系の collection の family の、weight に最初に合う字体。無ければ nullptr。
Font system_font(IDWriteFactory2 &factory, const wchar_t *name, DWRITE_FONT_WEIGHT weight)
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
    Font font;
    UINT32 index = 0;
    BOOL exists = FALSE;
    if (FAILED(factory.GetSystemFontCollection(&collection)) ||
        FAILED(collection->FindFamilyName(name, &index, &exists)) || exists == FALSE ||
        FAILED(collection->GetFontFamily(index, &family)) ||
        FAILED(family->GetFirstMatchingFont(weight, DWRITE_FONT_STRETCH_NORMAL,
                                            DWRITE_FONT_STYLE_NORMAL, &font)))
    {
        return nullptr;
    }
    return font;
}

// 覚えを 4,096 件で捨てた後も正規の字体の表と行番号は残り、同じ組の別の物は同じ行になる。
void check_rows_survive_forget(IDWriteFactory2 &factory)
{
    const auto regular = system_font(factory, L"Cascadia Code", DWRITE_FONT_WEIGHT_NORMAL);
    if (regular == nullptr)
    {
        expect(false, "Cascadia Code has a regular font");
        return;
    }
    const auto reads = std::make_shared<UINT32>(0U);
    const Font one = Microsoft::WRL::Make<ForwardingFont>(regular, reads);
    const Font two = Microsoft::WRL::Make<ForwardingFont>(regular, reads);
    win32::FallbackSegmentMemo memo;
    const auto row = memo.normalized(one);
    memo.remember(L"日本", row, 1, 2);
    for (std::size_t index = 0; index < win32::fallback_segment_memo_limit; ++index)
    {
        memo.remember(han(index), std::nullopt, 1, 1);
    }
    const bool forgot = memo.size() == 1 && !memo.find(L"日本").has_value();
    const auto again = memo.normalized(one);
    const auto other = memo.normalized(two);
    expect(forgot && row.has_value() && memo.rows() == 1 && again == row && other == row &&
               memo.row_font(row.value_or(0)) == one &&
               memo.row_family(row.value_or(0)) == L"Cascadia Code",
           "after the memo forgets beyond its limit the font rows and their numbers remain, and "
           "another object of the same family, weight, style and stretch maps to the same row");
}

// 正規の字体の表: 同じ組の別の物は同じ行で、行の字体は最初の物。family 名は新しい物のときだけ
// 読み、表にある物は読まない。違う weight は別の行。null は行にしない。
void check_normalized_rows(IDWriteFactory2 &factory)
{
    const auto regular = system_font(factory, L"Cascadia Code", DWRITE_FONT_WEIGHT_NORMAL);
    const auto bold = system_font(factory, L"Cascadia Code", DWRITE_FONT_WEIGHT_BOLD);
    if (regular == nullptr || bold == nullptr || regular->GetWeight() == bold->GetWeight())
    {
        expect(false, "Cascadia Code has a regular and a bold font");
        return;
    }
    const auto reads = std::make_shared<UINT32>(0U);
    const Font one = Microsoft::WRL::Make<ForwardingFont>(regular, reads);
    const Font two = Microsoft::WRL::Make<ForwardingFont>(regular, reads);
    const Font heavy = Microsoft::WRL::Make<ForwardingFont>(bold, reads);
    win32::FallbackSegmentMemo memo;
    const auto first = memo.normalized(one);
    const auto again = memo.normalized(one);
    const auto read_once = *reads;
    const auto second = memo.normalized(two);
    const auto third = memo.normalized(heavy);
    memo.remember(L"日本", first, 1, 2);
    memo.remember(L"中国", second, 1, 2);
    const auto japan = memo.find(L"日本");
    const auto china = memo.find(L"中国");
    const bool shared =
        first.has_value() && first == again && first == second && japan.has_value() &&
        china.has_value() && memo.font(japan.value()) == memo.font(china.value()) &&
        memo.row_font(first.value()) == one && memo.row_family(first.value()) == L"Cascadia Code";
    expect(shared && read_once == 1 && *reads == 3,
           "two objects of the same family, weight, style and stretch share one row and ComPtr, "
           "and the family name is read once per new object");
    expect(third.has_value() && third != first && memo.rows() == 2 &&
               memo.row_weight(third.value()) == bold->GetWeight() &&
               !memo.normalized(nullptr).has_value(),
           "another weight is another row and a null font has no row");
}

// coverage 経由: 替え玉が答える字体（Segoe UI・scale 1）を片ごとに別の物に包んでも、よく使う
// 漢字 6 字を繰り返した 600 単位の本文（片は 256・256・88 で、文字列は全部違う）は 1 つの範囲の
// 字体になり、family 名を読むのは字体選択の答えの数まで。同じ行の 2 回目は字体選択も family 名の
// 読みも 0 回。OS の字体選択に依らない（答えの字体は替え玉が決め、本文に空白を含めない）。
void check_normalized_coverage(IDWriteFactory2 &factory)
{
    const auto fallback = Microsoft::WRL::Make<ScriptedFontFallback>();
    const auto answer = system_font(factory, L"Segoe UI", DWRITE_FONT_WEIGHT_NORMAL);
    const auto format = scripted_format(factory, *fallback.Get());
    auto coverage =
        format != nullptr ? win32::PrimaryFontCoverage::of(factory, *format.Get()) : std::nullopt;
    if (!coverage.has_value() || answer == nullptr)
    {
        expect(false, "the wrapping format has a primary font coverage and Segoe UI");
        return;
    }
    fallback->answer_with(answer);
    fallback->wrap_fonts();
    std::wstring wide;
    const std::wstring_view common = L"日本語文字列";
    for (std::size_t index = 0; index < long_segment; ++index)
    {
        wide += common[index % common.size()];
    }
    const auto one = layout_of(factory, *format.Get(), wide);
    const auto first = assigned_calls(coverage.value(), *fallback.Get(), one, wide);
    const auto reads = fallback->family_reads();
    const auto second = assigned_calls(coverage.value(), *fallback.Get(),
                                       layout_of(factory, *format.Get(), wide), wide);
    std::array<wchar_t, name_capacity> name{};
    DWRITE_TEXT_RANGE range{};
    const bool joined = one != nullptr &&
                        SUCCEEDED(one->GetFontFamilyName(0, name.data(), name_capacity, &range)) &&
                        std::wstring{name.data()} == L"Segoe UI" && range.startPosition == 0 &&
                        range.length == wide.size();
    expect(first.value_or(0) > 0 && reads > 0 && reads <= first.value_or(0),
           "the family name is read at most once per font fallback answer, not per range");
    expect(joined, "adjacent pieces of one segment with distinct font objects make one range");
    expect(second.value_or(1) == 0 && fallback->family_reads() == reads,
           "the second pass of the same line reads no family name and calls no font fallback");
}
} // namespace

void verify_fallback_segment_memo()
{
    check_memo_type();
    const auto factory = directwrite_factory();
    if (factory == nullptr || !installed(*factory.Get(), L"Cascadia Code"))
    {
        unmeasured("the fallback segment memo through coverage needs Cascadia Code");
        return;
    }
    check_null_answers(*factory.Get());
    check_failures(*factory.Get());
    check_system_answers(*factory.Get());
    check_overflow(*factory.Get());
    check_normalized_rows(*factory.Get());
    check_rows_survive_forget(*factory.Get());
    check_normalized_coverage(*factory.Get());
}
} // namespace nenenib::tests::ui
