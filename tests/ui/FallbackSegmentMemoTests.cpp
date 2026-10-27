// Issue #884: 字体選択の区切りの片の文字列の覚え（ADR 0142 の追補）。同じ片の文字列を 2 回通すと
// 2 回目は OS の字体選択（替え玉）を呼ばず、違う文字列は呼び、null の答えも覚え、失敗は覚えず、
// 4096 件を超えると捨てて呼び直す。覚えの型の検査は字体に依らない。PrimaryFontCoverage を通す
// 検査は主字体（Cascadia Code）に依るので、無い機械では測れない（環境依存・QLT-013）。
#include "DirectWriteFactory.hpp"
#include "FallbackSegmentMemo.hpp"
#include "FontFallbackKey.hpp"
#include "PrimaryFontCoverage.hpp"
#include "ScriptedFontFallback.hpp"
#include "WindowChecks.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Format = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using Layout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;

constexpr FLOAT memo_font_size = 14.0F;
constexpr FLOAT memo_unbounded = 1.0e6F;
constexpr wchar_t han_first = 0x4E00;
constexpr std::size_t name_capacity = 128;
constexpr FLOAT remembered_scale = 1.5F;
constexpr UINT32 remembered_mapped = 3;

// 番号ごとに違う 1 文字の漢字（U+4E00 から）。
std::wstring han(std::size_t index)
{
    return std::wstring(1, static_cast<wchar_t>(han_first + index));
}

// 覚えの型だけの検査（字体に依らない）。
void check_memo_type()
{
    win32::FallbackSegmentMemo memo;
    memo.remember(L"日本", nullptr, remembered_scale, remembered_mapped);
    const auto found = memo.find(L"日本");
    expect(found.has_value() && memo.font(found.value()) == nullptr &&
               memo.scale(found.value()) == remembered_scale &&
               memo.mapped(found.value()) == remembered_mapped,
           "the segment memo keeps a null font, the scale and the mapped length of a text");
    expect(!memo.find(L"日").has_value() && !memo.find(L"日本語").has_value(),
           "the segment memo answers only the same text");
    memo.remember(L"", nullptr, 1, 1);
    memo.remember(std::wstring(win32::fallback_key_text_limit + 1, L'x'), nullptr, 1, 1);
    expect(memo.size() == 1, "the segment memo skips an empty text and one beyond the key limit");
    win32::FallbackSegmentMemo full;
    for (std::size_t index = 0; index < win32::fallback_segment_memo_limit; ++index)
    {
        full.remember(han(index), nullptr, 1, 1);
    }
    const bool held = full.size() == win32::fallback_segment_memo_limit &&
                      full.find(han(0)).has_value() &&
                      full.find(han(win32::fallback_segment_memo_limit - 1)).has_value();
    full.remember(han(win32::fallback_segment_memo_limit), nullptr, 1, 1);
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
}
} // namespace nenenib::tests::ui
