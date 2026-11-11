#include "StartScreenPainter.hpp"

#include "DevicePixels.hpp"
#include "DisplayText.hpp"
#include "KeycapPainter.hpp"
#include "PaletteRowKey.hpp"
#include "SaveState.hpp"
#include "StartEntryKind.hpp"
#include "TabTitle.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

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
constexpr std::string_view key_text = "ショートカット";
constexpr std::string_view empty_text =
    "まだ履歴がありません。ファイルを開くか、そのまま打ち始めてください";
constexpr std::string_view selected_mark = "▸";
constexpr std::string_view ellipsis = "…";
constexpr std::string_view new_file_text = "新しいファイル";
constexpr std::string_view new_file_detail = "そのまま打ち始める";
constexpr std::string_view open_file_text = "ファイルを開く";
constexpr std::string_view open_folder_text = "フォルダを開く";
constexpr std::string_view open_folder_detail = "ルートを選んでエクスプローラーを出す";
// 場所の全文の幅を測る欄の幅（物理画素・行に収まらない長さを測れればよい）。
constexpr std::int32_t measured_width = 8192;

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

// 欄 area の左から width と間 gap を空けた残り（名前の後ろの場所・補足の欄）。
[[nodiscard]] core::LayoutRect after(const core::LayoutRect &area, float width,
                                     std::int32_t gap) noexcept
{
    const auto left = area.left + static_cast<std::int32_t>(std::ceil(width)) + gap;
    return core::LayoutRect{std::min(left, area.right), area.top, area.right, area.bottom};
}

// UTF-8 の先頭のバイトが表すコードポイントのバイト数。
[[nodiscard]] std::size_t utf8_length(unsigned char lead) noexcept
{
    if (lead < 0x80U)
    {
        return 1;
    }
    if (lead < 0xE0U)
    {
        return 2;
    }
    return lead < 0xF0U ? 3 : 4;
}

// UTF-8 の text で、UTF-16 の先頭 units 単位を越えた最初のコードポイントの境界のバイト位置。
[[nodiscard]] std::size_t byte_after_units(std::string_view text, std::size_t units) noexcept
{
    std::size_t byte = 0;
    std::size_t counted = 0;
    while (byte < text.size() && counted < units)
    {
        const auto length = utf8_length(static_cast<unsigned char>(text.at(byte)));
        byte = std::min(byte + length, text.size());
        // 4 バイトのコードポイントは UTF-16 で 2 単位（サロゲートの対）。
        counted += length == 4 ? 2U : 1U;
    }
    return byte;
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

PaletteRowLayouts::TextLayout StartScreenPainter::obtained(std::string_view text, TextStyle style,
                                                           const core::LayoutRect &area)
{
    return layouts_.obtain(PaletteRowKey{style.first, style.second, text, area, text_.dpi},
                           [&] { return made(text, style, area); });
}

float StartScreenPainter::write(std::string_view text, TextStyle style,
                                const core::LayoutRect &area, core::RgbColor color)
{
    const auto shown = obtained(text, style, area);
    DWRITE_TEXT_METRICS metrics{};
    if (!shown || FAILED(shown->GetMetrics(&metrics)))
    {
        return static_cast<float>(core::width_of(area));
    }
    brush_.SetColor(to_color(color));
    target_.DrawTextLayout(
        D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top)), shown.Get(),
        &brush_, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    return metrics.widthIncludingTrailingWhitespace;
}

void StartScreenPainter::keycap(std::string_view key, const core::LayoutRect &area, bool from_left)
{
    const auto shown = obtained(key, {text_.key, PaletteTextRole::plain}, area);
    const float scale = static_cast<float>(text_.dpi) / static_cast<float>(core::reference_dpi);
    DWRITE_TEXT_METRICS metrics{};
    auto note = area;
    if (from_left && shown && SUCCEEDED(shown->GetMetrics(&metrics)))
    {
        // 枠の幅（字の幅と左右の余白）だけの欄にして、枠の左端を欄の左端に揃える。
        const auto box = static_cast<std::int32_t>(
            std::ceil(metrics.width + (text_.keycap.padding * scale * 2.0F)));
        note.right = std::min(area.left + box, area.right);
    }
    paint_keycap(KeycapCanvas{target_, brush_, scale}, shown.Get(), note, text_.keycap);
}

void StartScreenPainter::paint_headings(const core::StartScreenLayout &layout,
                                        const core::Palette &palette)
{
    const TextStyle heading{text_.heading, PaletteTextRole::plain};
    if (layout.recent_heading.has_value())
    {
        write(recent_text, heading, layout.recent_heading.value(), palette.muted);
    }
    if (layout.bookmark_heading.has_value())
    {
        write(bookmark_text, heading, layout.bookmark_heading.value(), palette.muted);
    }
    if (layout.empty_note.has_value())
    {
        write(empty_text, {text_.heading, PaletteTextRole::trimmed}, layout.empty_note.value(),
              palette.muted);
    }
    write(start_text, heading, layout.start_heading, palette.muted);
    write(key_text, heading, layout.key_heading, palette.muted);
}

void StartScreenPainter::paint_row(const core::StartEntry &entry, const core::StartRowLayout &row,
                                   bool selected, const core::Palette &palette)
{
    if (selected)
    {
        fill(row.line, palette.current_line);
        write(selected_mark, {text_.item, PaletteTextRole::plain}, row.mark, palette.muted);
    }
    switch (entry.kind)
    {
    case core::StartEntryKind::recent:
    case core::StartEntryKind::bookmark:
        if (entry.path.has_value())
        {
            paint_path(entry.path.value(), row.label, palette);
        }
        return;
    case core::StartEntryKind::new_file:
        paint_named(new_file_text, new_file_detail, row.label, palette);
        return;
    case core::StartEntryKind::open_file:
        paint_named(open_file_text, {}, row.label, palette);
        keycap(text_.open_key, row.label, false);
        return;
    case core::StartEntryKind::open_folder:
        paint_named(open_folder_text, open_folder_detail, row.label, palette);
        return;
    }
}

void StartScreenPainter::paint_named(std::string_view name, std::string_view detail,
                                     const core::LayoutRect &area, const core::Palette &palette)
{
    const auto width = write(name, {text_.item, PaletteTextRole::trimmed}, area, palette.text);
    if (detail.empty())
    {
        return;
    }
    const auto rest = after(area, width, core::to_pixels(core::start_detail_gap_dips, text_.dpi));
    if (core::width_of(rest) > 0)
    {
        write(detail, {text_.item, PaletteTextRole::trimmed}, rest, palette.muted);
    }
}

void StartScreenPainter::paint_path(const core::FilePath &path, const core::LayoutRect &area,
                                    const core::Palette &palette)
{
    // 名前と場所は Ctrl+P の面と同じ作り方（ADR 0057 の決定 7）。名前は切らず、名前だけで入り切ら
    // ないときだけ名前の末尾を「…」で切る（場所は出さない）。
    const auto name = core::tab_title_for(path, core::SaveState::saved);
    const auto width =
        write(name.text(), {text_.item, PaletteTextRole::trimmed}, area, palette.text);
    const auto folder = core::tab_folder_for(path);
    const auto rest = after(area, width, core::to_pixels(core::start_detail_gap_dips, text_.dpi));
    if (!folder.has_value() || core::width_of(rest) <= 0)
    {
        return;
    }
    const auto shown = head_trimmed(folder.value().text(), rest);
    write(shown, {text_.item, PaletteTextRole::plain}, rest, palette.muted);
}

std::string StartScreenPainter::head_trimmed(std::string_view text, const core::LayoutRect &area)
{
    // 全文の幅を広い欄の文字組みで測り、入らなければ「…」の幅を残して頭側を落とす位置を、同じ
    // 文字組みの当たり（HitTestPoint）で引く。どちらの文字組みも保持から引く。
    const TextStyle plain{text_.item, PaletteTextRole::plain};
    const core::LayoutRect wide{0, area.top, measured_width, area.bottom};
    const auto full = obtained(text, plain, wide);
    const auto mark = obtained(ellipsis, plain, wide);
    DWRITE_TEXT_METRICS whole{};
    DWRITE_TEXT_METRICS sign{};
    const auto room = static_cast<float>(core::width_of(area));
    if (!full || !mark || FAILED(full->GetMetrics(&whole)) || FAILED(mark->GetMetrics(&sign)) ||
        whole.width <= room)
    {
        return std::string(text);
    }
    BOOL trailing = FALSE;
    BOOL inside = FALSE;
    DWRITE_HIT_TEST_METRICS hit{};
    const float cut = whole.width - (room - sign.width);
    if (FAILED(full->HitTestPoint(cut, whole.height / 2.0F, &trailing, &inside, &hit)))
    {
        return std::string(ellipsis);
    }
    // 当たった字は欠けるので、その字の次の境界から残す。
    const std::size_t units = hit.textPosition + hit.length;
    return std::string(ellipsis) + std::string(text.substr(byte_after_units(text, units)));
}

void StartScreenPainter::paint_keys(const application::StartScreenView &view,
                                    const core::StartScreenLayout &layout,
                                    const core::Palette &palette)
{
    for (std::size_t index = 0; index < layout.keys.size(); ++index)
    {
        const auto &row = layout.keys.at(index);
        const auto &entry = view.keys.at(index);
        keycap(entry.key, row.key, true);
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
    Microsoft::WRL::ComPtr<IDWriteInlineObject> sign;
    const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    if (FAILED(layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)) ||
        FAILED(text_.dwrite->CreateEllipsisTrimmingSign(layout.Get(), sign.GetAddressOf())) ||
        FAILED(layout->SetTrimming(&trimming, sign.Get())))
    {
        return nullptr;
    }
    return layout;
}
} // namespace nenenib::ui::win32
