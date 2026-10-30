#include "PreviewTextLayouts.hpp"

#include "PreviewMetrics.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <array>
#include <dwrite_1.h>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr auto directwrite = RenderFailure::directwrite;
// 格子と同じ等幅の字体の候補（ADR 0140 の決定 6・D58）。
constexpr std::array<const wchar_t *, 2> code_fonts{L"Cascadia Mono", L"Consolas"};
// layout の高さの上限（物理画素）。高さは GetMetrics から読むので、ここは折り返しに効かない。
constexpr float unbounded_height = 1.0e7F;

[[nodiscard]] bool same_key(const PreviewTextLayout &entry, const core::PreviewBlock &block,
                            float width, const PreviewTextStyle &style)
{
    return entry.kind == block.kind && entry.level == block.level && entry.width == width &&
           entry.text == block.text && entry.spans == block.spans && entry.style == style;
}

[[nodiscard]] HRESULT styled(IDWriteTextLayout &layout, core::PreviewStyle style,
                             DWRITE_TEXT_RANGE range, const std::wstring &code_family)
{
    switch (style)
    {
    case core::PreviewStyle::emphasis:
    case core::PreviewStyle::image:
        return layout.SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range);
    case core::PreviewStyle::strong:
        return layout.SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range);
    case core::PreviewStyle::code:
        return layout.SetFontFamilyName(code_family.c_str(), range);
    case core::PreviewStyle::link:
        return layout.SetUnderline(TRUE, range);
    }
    std::unreachable();
}
} // namespace

bool PreviewTextLayouts::exists(const std::wstring &name) const
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    UINT32 index = 0;
    BOOL exists = FALSE;
    return SUCCEEDED(factory_->GetSystemFontCollection(&collection)) &&
           SUCCEEDED(collection->FindFamilyName(name.c_str(), &index, &exists)) && exists != FALSE;
}

// 機械にある family の既定の face が等幅か（IDWriteFont1::IsMonospacedFont）。
bool PreviewTextLayouts::monospaced(const std::wstring &name) const
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    Microsoft::WRL::ComPtr<IDWriteFontFamily> found;
    Microsoft::WRL::ComPtr<IDWriteFont> font;
    Microsoft::WRL::ComPtr<IDWriteFont1> version;
    UINT32 index = 0;
    BOOL exists = FALSE;
    return SUCCEEDED(factory_->GetSystemFontCollection(&collection)) &&
           SUCCEEDED(collection->FindFamilyName(name.c_str(), &index, &exists)) &&
           exists != FALSE && SUCCEEDED(collection->GetFontFamily(index, &found)) &&
           SUCCEEDED(found->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL,
                                                 DWRITE_FONT_STRETCH_NORMAL,
                                                 DWRITE_FONT_STYLE_NORMAL, &font)) &&
           SUCCEEDED(font.As(&version)) && version->IsMonospacedFont() != FALSE;
}

// コードは設定の字体が等幅ならそれ、でなければ格子と同じ候補（ADR 0140 の決定 6・D58）。
std::wstring PreviewTextLayouts::code_family(const std::wstring &requested) const
{
    if (monospaced(requested))
    {
        return requested;
    }
    const auto found =
        std::ranges::find_if(code_fonts, [this](const wchar_t *name) { return exists(name); });
    return found == code_fonts.end() ? requested : std::wstring{*found};
}

HRESULT PreviewTextLayouts::make_format(const std::wstring &family, float size, bool wrapped,
                                        TextFormat &format)
{
    format.Reset();
    const auto made = factory_->CreateTextFormat(family.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                                 DWRITE_FONT_STYLE_NORMAL,
                                                 DWRITE_FONT_STRETCH_NORMAL, size, L"", &format);
    if (FAILED(made))
    {
        return made;
    }
    return format->SetWordWrapping(wrapped ? DWRITE_WORD_WRAPPING_WRAP
                                           : DWRITE_WORD_WRAPPING_NO_WRAP);
}

HRESULT PreviewTextLayouts::prepare(const PreviewTextStyle &style)
{
    if (style_.has_value() && style_.value() == style)
    {
        return S_OK;
    }
    style_.reset();
    held_.clear();
    spacings_.clear();
    Microsoft::WRL::ComPtr<IUnknown> created;
    if (!factory_ && (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                                 __uuidof(IDWriteFactory), &created)) ||
                      FAILED(created.As(&factory_))))
    {
        return E_FAIL;
    }
    const auto requested = core::to_utf16(style.family);
    if (!requested)
    {
        return E_INVALIDARG;
    }
    code_family_ = code_family(requested.value());
    const auto size = preview_font_pixels(style);
    auto result = make_format(requested.value(), size, true, body_);
    if (SUCCEEDED(result))
    {
        result = make_format(code_family_, size, false, code_);
    }
    if (SUCCEEDED(result))
    {
        style_ = style;
    }
    return result;
}

// 書式の字の大きさ size での 1 行の自然な高さと baseline（空白 1 つの layout で測る・大きさごとに
// 1 回）。
std::optional<std::pair<float, float>> PreviewTextLayouts::natural(bool mono, float size)
{
    const auto known =
        std::ranges::find_if(spacings_, [mono, size](const Spacing &entry)
                             { return std::get<0>(entry) == mono && std::get<1>(entry) == size; });
    if (known != spacings_.end())
    {
        return std::pair{std::get<2>(*known), std::get<3>(*known)};
    }
    TextLayout probe;
    DWRITE_LINE_METRICS line{};
    UINT32 count = 0;
    if (FAILED(factory_->CreateTextLayout(L" ", 1, mono ? code_.Get() : body_.Get(),
                                          unbounded_height, unbounded_height, &probe)) ||
        FAILED(probe->SetFontSize(size, DWRITE_TEXT_RANGE{0, 1})) ||
        FAILED(probe->GetLineMetrics(&line, 1, &count)))
    {
        return std::nullopt;
    }
    spacings_.emplace_back(mono, size, line.height, line.baseline);
    return std::pair{line.height, line.baseline};
}

// 行高を本文と同じ比の固定にして、字を行の縦の中央に置く（BodyTextLayouts の set_spacing
// と同じ式）。
HRESULT PreviewTextLayouts::space(IDWriteTextLayout &layout, bool mono, float size, float line)
{
    const auto measured = natural(mono, size);
    if (!measured)
    {
        return E_FAIL;
    }
    const auto [height, baseline] = measured.value();
    return layout.SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, line,
                                 (line - height) / 2.0F + baseline);
}

HRESULT PreviewTextLayouts::decorate(IDWriteTextLayout &layout, const core::PreviewBlock &block,
                                     std::string_view shown)
{
    for (const auto &span : block.spans)
    {
        const auto begin = preview_units(shown, span.begin.value);
        const auto end = preview_units(shown, span.end.value);
        if (end > begin &&
            FAILED(styled(layout, span.style, DWRITE_TEXT_RANGE{begin, end - begin}, code_family_)))
        {
            return E_FAIL;
        }
    }
    return S_OK;
}

std::expected<PreviewTextLayout, RenderFailure>
PreviewTextLayouts::make(const core::PreviewBlock &block, float width,
                         const PreviewTextStyle &style)
{
    const auto shown = preview_shown_text(block);
    const auto wide = core::to_utf16(shown);
    if (!wide)
    {
        return std::unexpected(directwrite);
    }
    const bool mono = preview_monospaced(block.kind);
    const bool heading = block.kind == core::PreviewBlockKind::heading;
    const auto scale = heading ? preview_heading_scale(block.level) : 1.0F;
    const auto size = preview_font_pixels(style) * scale;
    const auto length = static_cast<UINT32>(wide.value().size());
    const DWRITE_TEXT_RANGE whole{0, std::max<UINT32>(length, 1)};
    TextLayout made;
    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(factory_->CreateTextLayout(wide.value().c_str(), length,
                                          mono ? code_.Get() : body_.Get(), width, unbounded_height,
                                          &made)) ||
        (heading && (FAILED(made->SetFontSize(size, whole)) ||
                     FAILED(made->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, whole)))) ||
        FAILED(space(*made.Get(), mono, size, preview_line_pixels(style) * scale)) ||
        FAILED(decorate(*made.Get(), block, shown)) || FAILED(made->GetMetrics(&metrics)))
    {
        return std::unexpected(directwrite);
    }
    ++created_;
    return PreviewTextLayout{block.text, block.spans, block.kind,      block.level,
                             width,      style,       std::move(made), metrics.height};
}

std::vector<PreviewTextLayout>::iterator PreviewTextLayouts::find(const core::PreviewBlock &block,
                                                                  float width,
                                                                  const PreviewTextStyle &style)
{
    return std::ranges::find_if(held_, [&](const PreviewTextLayout &entry)
                                { return same_key(entry, block, width, style); });
}

std::expected<PreviewTextLayouts::TextLayout, RenderFailure>
PreviewTextLayouts::get(const core::PreviewBlock &block, float width, const PreviewTextStyle &style)
{
    if (FAILED(prepare(style)))
    {
        return std::unexpected(directwrite);
    }
    const auto found = find(block, width, style);
    if (found != held_.end())
    {
        // 最近使った順の末尾へ回す（捨てるのは先頭から）。
        std::rotate(found, found + 1, held_.end());
        return held_.back().layout;
    }
    auto made = make(block, width, style);
    if (!made)
    {
        return std::unexpected(made.error());
    }
    if (held_.size() >= capacity)
    {
        held_.erase(held_.begin());
    }
    held_.push_back(std::move(made.value()));
    return held_.back().layout;
}

std::expected<float, RenderFailure> PreviewTextLayouts::measure(const core::PreviewBlock &block,
                                                                float width,
                                                                const PreviewTextStyle &style)
{
    if (FAILED(prepare(style)))
    {
        return std::unexpected(directwrite);
    }
    const auto found = find(block, width, style);
    if (found != held_.end())
    {
        return found->height;
    }
    const auto made = make(block, width, style);
    if (!made)
    {
        return std::unexpected(made.error());
    }
    return made.value().height;
}

std::expected<float, RenderFailure> PreviewTextLayouts::advance(const core::PreviewBlock &block,
                                                                float width,
                                                                const PreviewTextStyle &style)
{
    const auto layout = get(block, width, style);
    DWRITE_TEXT_METRICS metrics{};
    if (!layout || FAILED(layout.value()->GetMetrics(&metrics)))
    {
        return std::unexpected(directwrite);
    }
    return metrics.widthIncludingTrailingWhitespace;
}

std::size_t PreviewTextLayouts::held() const noexcept
{
    return held_.size();
}

std::size_t PreviewTextLayouts::created() const noexcept
{
    return created_;
}
} // namespace nenenib::ui::win32
