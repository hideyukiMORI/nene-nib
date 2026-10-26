#include "PrimaryFontCoverage.hpp"

#include "DisplayWidth.hpp"
#include "FallbackPiece.hpp"
#include "FontFallbackKey.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr std::size_t name_capacity = 128;

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
                                         Microsoft::WRL::ComPtr<IDWriteFontFallback> fallback,
                                         Microsoft::WRL::ComPtr<IDWriteTextAnalyzer> analyzer,
                                         application::TimingPort *timing)
    : timing_(timing), font_(std::move(font)), fallback_(std::move(fallback)), analyzer_(std::move(analyzer)),
      source_(
          Microsoft::WRL::Make<FallbackRunSource>(locale_of(format), format.GetReadingDirection())),
      sink_(Microsoft::WRL::Make<ScriptRunSink>()), family_(family_name_of(format)),
      weight_(format.GetFontWeight()), style_(format.GetFontStyle()),
      stretch_(format.GetFontStretch()), size_(format.GetFontSize()),
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

std::optional<PrimaryFontCoverage> PrimaryFontCoverage::of(IDWriteFactory &factory,
                                                           IDWriteTextFormat &format,
                                                           application::TimingPort *timing)
{
    auto font = primary_font(format);
    auto fallback = fallback_of(format);
    Microsoft::WRL::ComPtr<IDWriteTextAnalyzer> analyzer;
    if (font == nullptr || fallback == nullptr || FAILED(factory.CreateTextAnalyzer(&analyzer)))
    {
        return std::nullopt;
    }
    return PrimaryFontCoverage(format, std::move(font), std::move(fallback), std::move(analyzer),
                               timing);
}

void PrimaryFontCoverage::mark(core::Milestone milestone) const noexcept
{
    if (timing_ != nullptr)
    {
        timing_->mark(milestone);
    }
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

// 幅 0 の code point（結合文字・VS）は直前の文字に従う（ADR 0053 の 1 文字の境目）。
bool PrimaryFontCoverage::missing_at(char32_t value, bool joins, bool previous)
{
    return joins && core::display_width(value) == core::DisplayWidth::zero ? previous : !has(value);
}

// 区切りに主字体に無い文字が 1 つでもあるか（ADR 0142 の決定 2）。
bool PrimaryFontCoverage::has_missing(std::wstring_view wide, DWRITE_TEXT_RANGE segment)
{
    bool missing = false;
    for (UINT32 unit = segment.startPosition; unit < segment.startPosition + segment.length;)
    {
        const auto [value, units] = wide_code_point_at(wide, unit);
        missing = missing_at(value, unit != segment.startPosition, missing);
        if (missing)
        {
            return true;
        }
        unit += units;
    }
    return false;
}

HRESULT PrimaryFontCoverage::assign(IDWriteTextLayout &layout, std::wstring_view wide)
{
    mark(core::Milestone::fallback_begun);
    const auto assigned = assign_analyzed(layout, wide);
    mark(core::Milestone::fallback_assigned);
    return assigned;
}

HRESULT PrimaryFontCoverage::assign_analyzed(IDWriteTextLayout &layout, std::wstring_view wide)
{
    applied_ = 0;
    pending_font_.Reset();
    source_->reset(wide);
    // 文字種の解析に失敗すれば当てない（DirectWrite の代替のまま・決定 5）。
    const auto analyzed =
        analyzer_->AnalyzeScript(source_.Get(), 0, source_->length(), sink_.Get());
    mark(core::Milestone::script_analyzed);
    if (FAILED(analyzed))
    {
        static_cast<void>(sink_->take());
        return S_OK;
    }
    for (const auto segment : sink_->take())
    {
        if (applied_ >= fallback_range_limit)
        {
            break;
        }
        const auto assigned =
            has_missing(wide, segment) ? assign_segment(layout, wide, segment) : S_OK;
        if (FAILED(assigned))
        {
            return assigned;
        }
    }
    return flush(layout);
}

// 区切りの文字列を内容で決まる片（fallback_piece_length・#875）に切って OS
// の字体選択へ渡し、mappedLength の分ずつ持つ。 片は保持の鍵になるので FontFallbackCache
// の保持が効き、長い区切りの残りを毎回走査しない。 字体選択が失敗するか進まなければ、区切りの残りは
// DirectWrite の代替に任せる（決定 3・5）。
HRESULT PrimaryFontCoverage::assign_segment(IDWriteTextLayout &layout, std::wstring_view wide,
                                            DWRITE_TEXT_RANGE segment)
{
    for (UINT32 done = 0; done < segment.length;)
    {
        source_->reset(wide.substr(
            segment.startPosition + done,
            fallback_piece_length(wide, segment.startPosition + done, segment.length - done)));
        UINT32 mapped = 0;
        Microsoft::WRL::ComPtr<IDWriteFont> font;
        FLOAT scale = 1;
        if (FAILED(fallback_->MapCharacters(source_.Get(), 0, source_->length(), collection_.Get(),
                                            family_.c_str(), weight_, style_, stretch_, &mapped,
                                            &font, &scale)) ||
            mapped == 0)
        {
            return flush(layout);
        }
        mapped = std::min(mapped, source_->length());
        const DWRITE_TEXT_RANGE range{segment.startPosition + done, mapped};
        const auto held = font != nullptr ? hold(layout, *font.Get(), scale, range) : flush(layout);
        if (FAILED(held))
        {
            return held;
        }
        done += mapped;
    }
    return S_OK;
}

// 直前の範囲と同じ字体で隣り合う。
bool PrimaryFontCoverage::joins(IDWriteFont &font, FLOAT scale, DWRITE_TEXT_RANGE range)
{
    return pending_font_ != nullptr &&
           pending_range_.startPosition + pending_range_.length == range.startPosition &&
           pending_scale_ == scale && pending_font_->GetWeight() == font.GetWeight() &&
           pending_font_->GetStyle() == font.GetStyle() &&
           pending_font_->GetStretch() == font.GetStretch() && family_of(font) == pending_family_;
}

// 選ばれた字体の範囲を持つ。同じ字体の隣なら伸ばし、違えば前を当ててから持ち直す。主字体と
// 同じ字体なら当てない（DirectWrite が同じ字体を選ぶ）。
HRESULT PrimaryFontCoverage::hold(IDWriteTextLayout &layout, IDWriteFont &font, FLOAT scale,
                                  DWRITE_TEXT_RANGE range)
{
    if (joins(font, scale, range))
    {
        pending_range_.length += range.length;
        return S_OK;
    }
    const auto flushed = flush(layout);
    const bool primary = family_of(font) == family_ && font.GetWeight() == weight_ &&
                         font.GetStyle() == style_ && font.GetStretch() == stretch_ &&
                         scale == 1.0F;
    if (FAILED(flushed) || primary || applied_ >= fallback_range_limit)
    {
        return flushed;
    }
    pending_font_ = &font;
    pending_family_ = family_of(font);
    pending_range_ = range;
    pending_scale_ = scale;
    return S_OK;
}

HRESULT PrimaryFontCoverage::flush(IDWriteTextLayout &layout)
{
    if (pending_font_ == nullptr)
    {
        return S_OK;
    }
    ++applied_;
    const auto applied = apply(layout, *pending_font_.Get(), pending_scale_, pending_range_);
    pending_font_.Reset();
    return applied;
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
