#include "StartScreenPainter.hpp"

#include "DevicePixels.hpp"
#include "FilePath.hpp"
#include "KeycapPainter.hpp"
#include "PaletteRowKey.hpp"
#include "StartEntryKind.hpp"
#include "Utf16.hpp"

#include <optional>
#include <string>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr float full_channel = 255.0F;
// 採用案の文（docs/design/2026-10-28-start-screen-look.md・英語以外の文は後続・ADR 0172 の決定
// 8）。
constexpr std::string_view title_text = "NeNe Nib";
constexpr std::string_view recent_text = "最近使ったファイル";
constexpr std::string_view bookmark_text = "ブックマーク";
constexpr std::string_view start_text = "はじめる";
constexpr std::string_view key_text = "鍵";
constexpr std::string_view empty_text =
    "まだ履歴がありません。ファイルを開くか、そのまま打ち始めてください";
constexpr std::string_view selected_mark = "▸";
constexpr std::string_view new_file_text = "新しいファイル";
constexpr std::string_view new_file_side = "そのまま打ち始める";
constexpr std::string_view open_file_text = "ファイルを開く";
constexpr std::string_view open_folder_text = "フォルダを開く";
constexpr std::string_view open_folder_side = "ルートを選んでエクスプローラーを出す";

[[nodiscard]] D2D1_COLOR_F to_color(core::RgbColor color) noexcept
{
    return D2D1::ColorF(static_cast<float>(color.red) / full_channel,
                        static_cast<float>(color.green) / full_channel,
                        static_cast<float>(color.blue) / full_channel, 1.0F);
}

[[nodiscard]] D2D1_RECT_F to_rect(const core::LayoutRect &area) noexcept
{
    return D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                       static_cast<float>(area.right), static_cast<float>(area.bottom));
}

// 行の名前と右端の補足（補足が無ければ空）。パスの行はパスの全文で、末尾を「…」で切るのは
// 文字組み（trimmed）。
[[nodiscard]] std::pair<std::string_view, std::string_view>
row_texts(const core::StartEntry &entry) noexcept
{
    switch (entry.kind)
    {
    case core::StartEntryKind::recent:
    case core::StartEntryKind::bookmark:
        return {entry.path.has_value() ? entry.path.value().text() : std::string_view{}, {}};
    case core::StartEntryKind::new_file:
        return {new_file_text, new_file_side};
    case core::StartEntryKind::open_file:
        return {open_file_text, {}};
    case core::StartEntryKind::open_folder:
        return {open_folder_text, open_folder_side};
    }
    return {};
}
} // namespace

StartScreenPainter::StartScreenPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                                       const StartScreenText &text,
                                       PaletteRowLayouts &layouts) noexcept
    : target_(target), brush_(brush), text_(text), layouts_(layouts)
{
}

void StartScreenPainter::paint(const application::StartScreenView &view,
                               const core::StartScreenLayout &layout, const core::Palette &palette,
                               const core::LayoutRect &clip)
{
    if (core::width_of(clip) <= 0 || core::height_of(clip) <= 0)
    {
        return;
    }
    target_.PushAxisAlignedClip(to_rect(clip), D2D1_ANTIALIAS_MODE_ALIASED);
    write(title_text, {text_.title, PaletteTextRole::plain}, layout.title, palette.muted);
    paint_headings(layout, palette);
    for (std::size_t index = 0; index < layout.rows.size() && index < view.entries.size(); ++index)
    {
        paint_row(view.entries.at(index), layout.rows.at(index), view.selected == index, palette);
    }
    paint_keys(view, layout, palette);
    target_.PopAxisAlignedClip();
}

void StartScreenPainter::fill(const core::LayoutRect &area, core::RgbColor color)
{
    brush_.SetColor(to_color(color));
    target_.FillRectangle(to_rect(area), &brush_);
}

void StartScreenPainter::write(std::string_view text, TextStyle style, const core::LayoutRect &area,
                               core::RgbColor color)
{
    const auto shown =
        layouts_.obtain(PaletteRowKey{style.first, style.second, text, area, text_.dpi},
                        [&] { return made(text, style, area); });
    if (!shown)
    {
        return;
    }
    brush_.SetColor(to_color(color));
    target_.DrawTextLayout(
        D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top)), shown.Get(),
        &brush_, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void StartScreenPainter::keycap(std::string_view key, const core::LayoutRect &area)
{
    const auto shown =
        layouts_.obtain(PaletteRowKey{text_.key, PaletteTextRole::plain, key, area, text_.dpi},
                        [&] { return made(key, {text_.key, PaletteTextRole::plain}, area); });
    paint_keycap(
        KeycapCanvas{target_, brush_,
                     static_cast<float>(text_.dpi) / static_cast<float>(core::reference_dpi)},
        shown.Get(), area, text_.keycap);
}

void StartScreenPainter::paint_headings(const core::StartScreenLayout &layout,
                                        const core::Palette &palette)
{
    if (layout.recent_heading.has_value())
    {
        write(recent_text, {text_.heading, PaletteTextRole::plain}, layout.recent_heading.value(),
              palette.muted);
    }
    if (layout.bookmark_heading.has_value())
    {
        write(bookmark_text, {text_.heading, PaletteTextRole::plain},
              layout.bookmark_heading.value(), palette.muted);
    }
    if (layout.empty_note.has_value())
    {
        write(empty_text, {text_.heading, PaletteTextRole::trimmed}, layout.empty_note.value(),
              palette.muted);
    }
    write(start_text, {text_.heading, PaletteTextRole::plain}, layout.start_heading, palette.muted);
    write(key_text, {text_.heading, PaletteTextRole::plain}, layout.key_heading, palette.muted);
}

void StartScreenPainter::paint_row(const core::StartEntry &entry, const core::StartRowLayout &row,
                                   bool selected, const core::Palette &palette)
{
    if (selected)
    {
        fill(row.line, palette.current_line);
        write(selected_mark, {text_.item, PaletteTextRole::plain}, row.mark, palette.muted);
    }
    const auto [label, side] = row_texts(entry);
    write(label, {text_.item, PaletteTextRole::trimmed}, row.label, palette.text);
    if (entry.kind == core::StartEntryKind::open_file)
    {
        keycap(text_.open_key, row.side);
        return;
    }
    if (!side.empty())
    {
        write(side, {text_.side, PaletteTextRole::trimmed}, row.side, palette.muted);
    }
}

void StartScreenPainter::paint_keys(const application::StartScreenView &view,
                                    const core::StartScreenLayout &layout,
                                    const core::Palette &palette)
{
    for (std::size_t index = 0; index < layout.keys.size(); ++index)
    {
        const auto &row = layout.keys.at(index);
        const auto &entry = view.keys.at(index);
        keycap(entry.key, row.key);
        write(entry.body_name, {text_.key_label, PaletteTextRole::trimmed}, row.label,
              palette.muted);
    }
}

PaletteRowLayouts::TextLayout StartScreenPainter::made(std::string_view text, TextStyle style,
                                                       const core::LayoutRect &area)
{
    const auto [format, role] = style;
    const std::wstring wide = core::to_utf16(text).value_or(std::wstring{});
    PaletteRowLayouts::TextLayout layout;
    if (text_.dwrite == nullptr || format == nullptr ||
        FAILED(text_.dwrite->CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()),
                                              format, static_cast<float>(core::width_of(area)),
                                              static_cast<float>(core::height_of(area)),
                                              layout.GetAddressOf())))
    {
        return nullptr;
    }
    switch (role)
    {
    case PaletteTextRole::plain:
    case PaletteTextRole::trailing:
        return layout;
    case PaletteTextRole::trimmed:
        break;
    }
    // 折り返さず幅で切る（文字単位の省略記号「…」・タブの題名と同じ規則）。
    Microsoft::WRL::ComPtr<IDWriteInlineObject> ellipsis;
    const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    if (FAILED(layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)) ||
        FAILED(text_.dwrite->CreateEllipsisTrimmingSign(layout.Get(), ellipsis.GetAddressOf())) ||
        FAILED(layout->SetTrimming(&trimming, ellipsis.Get())))
    {
        return nullptr;
    }
    return layout;
}
} // namespace nenenib::ui::win32
