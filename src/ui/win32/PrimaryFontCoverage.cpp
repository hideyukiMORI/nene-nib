#include "PrimaryFontCoverage.hpp"

#include "DisplayWidth.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr std::size_t name_capacity = 128;
constexpr wchar_t high_surrogate_first = 0xD800;
constexpr wchar_t high_surrogate_last = 0xDBFF;
constexpr wchar_t low_surrogate_first = 0xDC00;
constexpr wchar_t low_surrogate_last = 0xDFFF;
constexpr char32_t surrogate_base = 0x10000;
constexpr unsigned surrogate_shift = 10;

// unit の位置の code point と、その UTF-16 の単位数。対になっていないサロゲートはそのまま 1 単位。
[[nodiscard]] std::pair<char32_t, UINT32> code_point_at(std::wstring_view wide, std::size_t unit)
{
    const auto first = wide[unit];
    if (first < high_surrogate_first || first > high_surrogate_last || unit + 1 >= wide.size() ||
        wide[unit + 1] < low_surrogate_first || wide[unit + 1] > low_surrogate_last)
    {
        return {static_cast<char32_t>(first), 1U};
    }
    const auto high = static_cast<char32_t>(first - high_surrogate_first);
    const auto low = static_cast<char32_t>(wide[unit + 1] - low_surrogate_first);
    return {surrogate_base + (high << surrogate_shift) + low, 2U};
}

// 書式の family の、書式の weight / stretch / style に最初に合う字体。family が無ければ nullptr。
[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFont> primary_font(IDWriteTextFormat &format)
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
    Microsoft::WRL::ComPtr<IDWriteFont> font;
    std::array<wchar_t, name_capacity> name{};
    UINT32 index = 0;
    BOOL exists = FALSE;
    if (FAILED(format.GetFontCollection(&collection)) || collection == nullptr ||
        FAILED(format.GetFontFamilyName(name.data(), name_capacity)) ||
        FAILED(collection->FindFamilyName(name.data(), &index, &exists)) || exists == FALSE ||
        FAILED(collection->GetFontFamily(index, &family)) ||
        FAILED(family->GetFirstMatchingFont(format.GetFontWeight(), format.GetFontStretch(),
                                            format.GetFontStyle(), &font)))
    {
        return nullptr;
    }
    return font;
}

// 書式に付いた字体選択（本文では FontFallbackCache・ADR 0071）。無ければ nullptr。
[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFallback> fallback_of(IDWriteTextFormat &format)
{
    Microsoft::WRL::ComPtr<IDWriteTextFormat1> version;
    Microsoft::WRL::ComPtr<IDWriteFontFallback> fallback;
    if (FAILED(Microsoft::WRL::ComPtr<IDWriteTextFormat>(&format).As(&version)) ||
        FAILED(version->GetFontFallback(&fallback)))
    {
        return nullptr;
    }
    return fallback;
}

[[nodiscard]] std::wstring locale_of(IDWriteTextFormat &format)
{
    std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> name{};
    return SUCCEEDED(format.GetLocaleName(name.data(), LOCALE_NAME_MAX_LENGTH)) ? name.data()
                                                                                : std::wstring{};
}

[[nodiscard]] std::wstring family_name_of(IDWriteTextFormat &format)
{
    std::array<wchar_t, name_capacity> name{};
    return SUCCEEDED(format.GetFontFamilyName(name.data(), name_capacity)) ? name.data()
                                                                           : std::wstring{};
}

// 字体の family の名前（en-us、無ければ最初の名前）。読めなければ空。
[[nodiscard]] std::wstring english_family(IDWriteFont &font)
{
    Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names;
    UINT32 index = 0;
    BOOL exists = FALSE;
    UINT32 length = 0;
    if (FAILED(font.GetFontFamily(&family)) || FAILED(family->GetFamilyNames(&names)) ||
        FAILED(names->FindLocaleName(L"en-us", &index, &exists)))
    {
        return {};
    }
    index = exists != FALSE ? index : 0;
    if (FAILED(names->GetStringLength(index, &length)))
    {
        return {};
    }
    std::wstring name(length + 1, L'\0');
    if (FAILED(names->GetString(index, name.data(), length + 1)))
    {
        return {};
    }
    name.resize(length);
    return name;
}
} // namespace

PrimaryFontCoverage::PrimaryFontCoverage(IDWriteTextFormat &format,
                                         Microsoft::WRL::ComPtr<IDWriteFont> font,
                                         Microsoft::WRL::ComPtr<IDWriteFontFallback> fallback)
    : font_(std::move(font)), fallback_(std::move(fallback)),
      source_(
          Microsoft::WRL::Make<FallbackRunSource>(locale_of(format), format.GetReadingDirection())),
      family_(family_name_of(format)), weight_(format.GetFontWeight()),
      style_(format.GetFontStyle()), stretch_(format.GetFontStretch()), size_(format.GetFontSize()),
      known_(std::make_unique<std::bitset<basic_plane>>()),
      present_(std::make_unique<std::bitset<basic_plane>>())
{
    static_cast<void>(format.GetFontCollection(&collection_));
    Microsoft::WRL::ComPtr<IDWriteFontFace> face;
    if (SUCCEEDED(font_->CreateFontFace(&face)))
    {
        static_cast<void>(face.As(&face_));
    }
}

std::optional<PrimaryFontCoverage> PrimaryFontCoverage::of(IDWriteTextFormat &format)
{
    auto font = primary_font(format);
    auto fallback = fallback_of(format);
    if (font == nullptr || fallback == nullptr)
    {
        return std::nullopt;
    }
    return PrimaryFontCoverage(format, std::move(font), std::move(fallback));
}

bool PrimaryFontCoverage::query(char32_t value) const
{
    if (face_ != nullptr)
    {
        return face_->HasCharacter(value) != FALSE;
    }
    BOOL exists = FALSE;
    return SUCCEEDED(font_->HasCharacter(value, &exists)) && exists != FALSE;
}

bool PrimaryFontCoverage::has(char32_t value)
{
    if (value < basic_plane)
    {
        if (!known_->test(value))
        {
            known_->set(value);
            present_->set(value, query(value));
        }
        return present_->test(value);
    }
    const auto found = astral_.find(value);
    if (found != astral_.end())
    {
        return found->second;
    }
    const bool present = query(value);
    astral_.emplace(value, present);
    return present;
}

// 幅 0 の code point（結合文字・VS）は直前の文字の区間に含める（ADR 0053 の 1 文字の境目）。
bool PrimaryFontCoverage::missing_at(char32_t value, bool joins, bool previous)
{
    return joins && core::display_width(value) == core::DisplayWidth::zero ? previous : !has(value);
}

std::vector<DWRITE_TEXT_RANGE> PrimaryFontCoverage::missing_runs(std::wstring_view wide)
{
    std::vector<DWRITE_TEXT_RANGE> runs;
    bool missing = false;
    UINT32 start = 0;
    UINT32 unit = 0;
    while (unit < wide.size() && runs.size() < fallback_range_limit)
    {
        const auto [value, units] = code_point_at(wide, unit);
        const bool now = missing_at(value, unit != 0, missing);
        start = now && !missing ? unit : start;
        if (missing && !now)
        {
            runs.push_back(DWRITE_TEXT_RANGE{start, unit - start});
        }
        missing = now;
        unit += units;
    }
    if (missing && runs.size() < fallback_range_limit)
    {
        runs.push_back(DWRITE_TEXT_RANGE{start, unit - start});
    }
    return runs;
}

HRESULT PrimaryFontCoverage::assign(IDWriteTextLayout &layout, std::wstring_view wide)
{
    for (const auto run : missing_runs(wide))
    {
        const auto assigned = assign_run(layout, wide, run);
        if (FAILED(assigned))
        {
            return assigned;
        }
    }
    return S_OK;
}

// 区間の残りを区間の文字列として OS の字体選択へ渡し、mappedLength の分だけ当てる。字体選択が
// 失敗するか進まなければ、残りは DirectWrite の代替に任せる（決定 5）。
HRESULT PrimaryFontCoverage::assign_run(IDWriteTextLayout &layout, std::wstring_view wide,
                                        DWRITE_TEXT_RANGE run)
{
    for (UINT32 done = 0; done < run.length;)
    {
        source_->reset(wide.substr(run.startPosition + done, run.length - done));
        UINT32 mapped = 0;
        Microsoft::WRL::ComPtr<IDWriteFont> font;
        FLOAT scale = 1;
        if (FAILED(fallback_->MapCharacters(source_.Get(), 0, source_->length(), collection_.Get(),
                                            family_.c_str(), weight_, style_, stretch_, &mapped,
                                            &font, &scale)) ||
            mapped == 0)
        {
            return S_OK;
        }
        mapped = std::min(mapped, run.length - done);
        const DWRITE_TEXT_RANGE range{run.startPosition + done, mapped};
        const auto applied = font != nullptr ? apply(layout, *font.Get(), scale, range) : S_OK;
        if (FAILED(applied))
        {
            return applied;
        }
        done += mapped;
    }
    return S_OK;
}

// 選ばれた字体の family 名を範囲に当て、書式と違う weight / style / stretch と scale も当てる。
HRESULT PrimaryFontCoverage::apply(IDWriteTextLayout &layout, IDWriteFont &font, FLOAT scale,
                                   DWRITE_TEXT_RANGE range)
{
    const auto &family = family_of(font);
    if (family.empty())
    {
        return S_OK;
    }
    auto result = layout.SetFontFamilyName(family.c_str(), range);
    if (SUCCEEDED(result) && font.GetWeight() != weight_)
    {
        result = layout.SetFontWeight(font.GetWeight(), range);
    }
    if (SUCCEEDED(result) && font.GetStyle() != style_)
    {
        result = layout.SetFontStyle(font.GetStyle(), range);
    }
    if (SUCCEEDED(result) && font.GetStretch() != stretch_)
    {
        result = layout.SetFontStretch(font.GetStretch(), range);
    }
    if (SUCCEEDED(result) && scale != 1.0F)
    {
        result = layout.SetFontSize(size_ * scale, range);
    }
    return result;
}

const std::wstring &PrimaryFontCoverage::family_of(IDWriteFont &font)
{
    if (named_.Get() != &font)
    {
        named_ = &font;
        named_family_ = english_family(font);
    }
    return named_family_;
}
} // namespace nenenib::ui::win32
