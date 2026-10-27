#include "PrimaryFontCoverage.hpp"

#include "DisplayWidth.hpp"
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

// unit の位置で前の文字が続く（サロゲートの対の後半か、幅 0 の code point）。
[[nodiscard]] bool continues_at(std::wstring_view wide, std::size_t unit)
{
    const auto value = wide[unit];
    return (value >= low_surrogate_first && value <= low_surrogate_last) ||
           core::display_width(code_point_at(wide, unit).first) == core::DisplayWidth::zero;
}

// 区切りの start から字体選択へ 1 回に渡す長さ。保持の鍵の上限までに切り、切り目がサロゲートの対
// や幅 0 の code point（直前の文字に従う・ADR 0053）の手前に来るなら、その文字の前まで戻す。
[[nodiscard]] UINT32 piece_length(std::wstring_view wide, UINT32 start, UINT32 remaining)
{
    auto length = std::min(remaining, fallback_key_text_limit);
    while (length > 1 && length < remaining && continues_at(wide, start + length))
    {
        --length;
    }
    return length;
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
} // namespace

PrimaryFontCoverage::PrimaryFontCoverage(IDWriteTextFormat &format,
                                         Microsoft::WRL::ComPtr<IDWriteFont> font,
                                         Microsoft::WRL::ComPtr<IDWriteFontFallback> fallback,
                                         Microsoft::WRL::ComPtr<IDWriteTextAnalyzer> analyzer)
    : font_(std::move(font)), fallback_(std::move(fallback)), analyzer_(std::move(analyzer)),
      source_(
          Microsoft::WRL::Make<FallbackRunSource>(locale_of(format), format.GetReadingDirection())),
      sink_(Microsoft::WRL::Make<ScriptRunSink>()), family_(family_name_of(format)),
      weight_(format.GetFontWeight()), style_(format.GetFontStyle()),
      stretch_(format.GetFontStretch()), size_(format.GetFontSize()),
      known_(std::make_unique<std::bitset<basic_plane>>()),
      present_(std::make_unique<std::bitset<basic_plane>>()),
      memo_(std::make_unique<FallbackSegmentMemo>())
{
    static_cast<void>(format.GetFontCollection(&collection_));
    Microsoft::WRL::ComPtr<IDWriteFontFace> face;
    if (SUCCEEDED(font_->CreateFontFace(&face)))
    {
        static_cast<void>(face.As(&face_));
    }
}

std::optional<PrimaryFontCoverage> PrimaryFontCoverage::of(IDWriteFactory &factory,
                                                           IDWriteTextFormat &format)
{
    auto font = primary_font(format);
    auto fallback = fallback_of(format);
    Microsoft::WRL::ComPtr<IDWriteTextAnalyzer> analyzer;
    if (font == nullptr || fallback == nullptr || FAILED(factory.CreateTextAnalyzer(&analyzer)))
    {
        return std::nullopt;
    }
    return PrimaryFontCoverage(format, std::move(font), std::move(fallback), std::move(analyzer));
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
        const auto [value, units] = code_point_at(wide, unit);
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
    applied_ = 0;
    pending_font_.reset();
    source_->reset(wide);
    // 文字種の解析に失敗すれば当てない（DirectWrite の代替のまま・決定 5）。
    if (FAILED(analyzer_->AnalyzeScript(source_.Get(), 0, source_->length(), sink_.Get())))
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

// 区切りの文字列を保持の鍵の上限までの片に切って OS の字体選択へ渡し、mappedLength の分ずつ持つ。
// 片は保持の鍵になるので FontFallbackCache の保持が効き、長い区切りの残りを毎回走査しない。
// 字体選択が失敗するか進まなければ、区切りの残りは DirectWrite の代替に任せる（決定 3・5）。
HRESULT PrimaryFontCoverage::assign_segment(IDWriteTextLayout &layout, std::wstring_view wide,
                                            DWRITE_TEXT_RANGE segment)
{
    for (UINT32 done = 0; done < segment.length;)
    {
        const auto start = segment.startPosition + done;
        const auto piece = wide.substr(start, piece_length(wide, start, segment.length - done));
        UINT32 mapped = 0;
        std::optional<std::size_t> font;
        FLOAT scale = 1;
        if (FAILED(map_piece(piece, mapped, font, scale)) || mapped == 0)
        {
            return flush(layout);
        }
        mapped = std::min(mapped, static_cast<UINT32>(piece.size()));
        const DWRITE_TEXT_RANGE range{start, mapped};
        const auto held =
            font.has_value() ? hold(layout, font.value(), scale, range) : flush(layout);
        if (FAILED(held))
        {
            return held;
        }
        done += mapped;
    }
    return S_OK;
}

// 片の字体選択の答え。同じ片の文字列を覚えていれば OS の字体選択を呼ばずにその答えを返し、
// 覚えていなければ呼んで S_OK で進んだ答えだけを覚える（失敗と mappedLength 0 は覚えない・
// ADR 0142 の追補）。字体は正規の字体の表の行にしてから覚え・返す（#889）。
HRESULT PrimaryFontCoverage::map_piece(std::wstring_view piece, UINT32 &mapped,
                                       std::optional<std::size_t> &font, FLOAT &scale)
{
    const auto remembered = memo_->find(piece);
    if (remembered.has_value())
    {
        mapped = memo_->mapped(remembered.value());
        font = memo_->font(remembered.value());
        scale = memo_->scale(remembered.value());
        return S_OK;
    }
    source_->reset(piece);
    Microsoft::WRL::ComPtr<IDWriteFont> chosen;
    const auto result = fallback_->MapCharacters(source_.Get(), 0, source_->length(),
                                                 collection_.Get(), family_.c_str(), weight_,
                                                 style_, stretch_, &mapped, &chosen, &scale);
    if (FAILED(result))
    {
        return result;
    }
    font = memo_->normalized(chosen);
    if (result == S_OK && mapped != 0)
    {
        memo_->remember(piece, font, scale, mapped);
    }
    return result;
}

// 直前の範囲と同じ字体（正規の表の同じ行・同じ scale）で隣り合う。行が同じことは family 名・
// weight・style・stretch が同じことなので、COM を呼ばずに比べる（#889）。
bool PrimaryFontCoverage::joins(std::size_t font, FLOAT scale, DWRITE_TEXT_RANGE range) const
{
    return pending_font_.has_value() && pending_font_.value() == font &&
           pending_range_.startPosition + pending_range_.length == range.startPosition &&
           pending_scale_ == scale;
}

// 書式の主字体と同じ字体（DirectWrite が同じ字体を選ぶので当てない）。
bool PrimaryFontCoverage::primary(std::size_t font, FLOAT scale) const
{
    return scale == 1.0F && memo_->row_weight(font) == weight_ &&
           memo_->row_style(font) == style_ && memo_->row_stretch(font) == stretch_ &&
           memo_->row_family(font) == family_;
}

// 選ばれた字体の範囲を持つ。同じ字体の隣なら伸ばし、違えば前を当ててから持ち直す。主字体と
// 同じ字体なら当てない（DirectWrite が同じ字体を選ぶ）。
HRESULT PrimaryFontCoverage::hold(IDWriteTextLayout &layout, std::size_t font, FLOAT scale,
                                  DWRITE_TEXT_RANGE range)
{
    if (joins(font, scale, range))
    {
        pending_range_.length += range.length;
        return S_OK;
    }
    const auto flushed = flush(layout);
    if (FAILED(flushed) || primary(font, scale) || applied_ >= fallback_range_limit)
    {
        return flushed;
    }
    pending_font_ = font;
    pending_range_ = range;
    pending_scale_ = scale;
    return S_OK;
}

HRESULT PrimaryFontCoverage::flush(IDWriteTextLayout &layout)
{
    if (!pending_font_.has_value())
    {
        return S_OK;
    }
    ++applied_;
    const auto applied = apply(layout, pending_font_.value(), pending_scale_, pending_range_);
    pending_font_.reset();
    return applied;
}

// 選ばれた字体（正規の表の行）の family 名を範囲に当て、書式と違う weight / style / stretch と
// scale も当てる。行の family 名は空でない（読めない字体は行にしない）。
HRESULT PrimaryFontCoverage::apply(IDWriteTextLayout &layout, std::size_t font, FLOAT scale,
                                   DWRITE_TEXT_RANGE range) const
{
    const auto weight = memo_->row_weight(font);
    const auto style = memo_->row_style(font);
    const auto stretch = memo_->row_stretch(font);
    auto result = layout.SetFontFamilyName(memo_->row_family(font).c_str(), range);
    if (SUCCEEDED(result) && weight != weight_)
    {
        result = layout.SetFontWeight(weight, range);
    }
    if (SUCCEEDED(result) && style != style_)
    {
        result = layout.SetFontStyle(style, range);
    }
    if (SUCCEEDED(result) && stretch != stretch_)
    {
        result = layout.SetFontStretch(stretch, range);
    }
    if (SUCCEEDED(result) && scale != 1.0F)
    {
        result = layout.SetFontSize(size_ * scale, range);
    }
    return result;
}
} // namespace nenenib::ui::win32
