#include "SettingsPanelPainter.hpp"

#include "CostTier.hpp"
#include "DevicePixels.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "FeatureCosts.hpp"
#include "KeyChord.hpp"
#include "KeycapPainter.hpp"
#include "KeycapStyle.hpp"
#include "LanguageServerRowAction.hpp"
#include "LanguageServerRowState.hpp"
#include "LanguageServerRowView.hpp"
#include "OperationBindings.hpp"
#include "PaletteRowKey.hpp"
#include "PaletteTextRole.hpp"
#include "SettingsControl.hpp"
#include "SettingsItemKind.hpp"
#include "SettingsLayoutRow.hpp"
#include "SettingsPanel.hpp"
#include "SettingsPreset.hpp"
#include "SettingsRows.hpp"
#include "SettingsSection.hpp"
#include "SettingsSubject.hpp"
#include "SettingsTextWidths.hpp"
#include "UiTexts.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
namespace
{
constexpr float full_channel = 255.0F;
// 1 行の文字組みの箱（折り返さないので幅は箱に依らず、縦は箱の中央に組む）。
constexpr std::int32_t line_width = 4096;
constexpr std::int32_t line_height = 256;
// 角丸（DIP）: 行と要約と探す欄は 8、選ぶ欄とボタンは 6。札は高さの半分。
constexpr float row_radius_dips = 8.0F;
constexpr float box_radius_dips = 6.0F;
constexpr float chip_radius_dips = 13.0F;
constexpr float switch_radius_dips = 11.0F;
constexpr float knob_radius_dips = 8.0F;
constexpr float knob_inset_dips = 3.0F;
constexpr float dot_radius_dips = 3.0F;
constexpr float dot_step_dips = 8.0F;
constexpr float glyph_stroke_dips = 1.25F;
constexpr float glyph_half_dips = 4.0F;
constexpr std::int32_t box_padding_dips = 10;
constexpr std::int32_t title_top_dips = 10;
constexpr std::int32_t title_height_dips = 18;
constexpr std::int32_t hint_gap_dips = 2;
constexpr std::int32_t tier_gap_dips = 8;
constexpr std::int32_t summary_padding_dips = core::settings_summary_inset_dips;
constexpr std::int32_t summary_line_dips = core::settings_summary_line_dips;
constexpr std::int32_t summary_note_dips = 16;
constexpr std::int32_t icon_dips = 14;
constexpr KeycapStyle chord_style_shape{6.0F, 20.0F, 4.0F, {}, {}, {}};

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

[[nodiscard]] D2D1_POINT_2F centre_of(const core::LayoutRect &area) noexcept
{
    return D2D1::Point2F(static_cast<float>(area.left + area.right) / 2.0F,
                         static_cast<float>(area.top + area.bottom) / 2.0F);
}

[[nodiscard]] core::UiMessage preset_message(core::SettingsPreset preset) noexcept
{
    switch (preset)
    {
    case core::SettingsPreset::speed_first:
        return core::UiMessage::settings_preset_speed_first;
    case core::SettingsPreset::recommended:
        return core::UiMessage::settings_preset_recommended;
    case core::SettingsPreset::all_on:
        return core::UiMessage::settings_preset_all_on;
    }
    std::unreachable();
}

[[nodiscard]] core::LayoutRect inset(const core::LayoutRect &area, std::int32_t sides) noexcept
{
    return core::LayoutRect{area.left + sides, area.top, area.right - sides, area.bottom};
}
} // namespace

SettingsPanelPainter::SettingsPanelPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                                           const SettingsPanelText &text,
                                           PaletteRowLayouts &layouts) noexcept
    : target_(target), brush_(brush), text_(text), layouts_(layouts)
{
}

std::int32_t SettingsPanelPainter::pixels(std::int32_t dips) const noexcept
{
    return core::to_pixels(dips, text_.dpi);
}

float SettingsPanelPainter::scaled(float dips) const noexcept
{
    return dips * static_cast<float>(text_.dpi) / static_cast<float>(core::reference_dpi);
}

std::string_view SettingsPanelPainter::said(core::UiMessage message) const noexcept
{
    return core::ui_text(message, text_.language);
}

core::SettingsLayout SettingsPanelPainter::arranged(const application::SettingsView &view,
                                                    const core::LayoutRect &area) const
{
    std::vector<core::SettingsLayoutRow> rows;
    rows.reserve(view.rows.size() + view.servers.size() + 1);
    for (const auto &row : view.rows)
    {
        rows.push_back(core::SettingsLayoutRow{
            row.row.section, core::settings_control_of(row.row.subject, row.switchable)});
    }
    // 「言語サーバー」の節の行（押す物があれば action）と、畳んだ行があれば「ほかの N
    // 言語を見る」。
    for (const auto &server : view.servers)
    {
        rows.push_back(
            core::SettingsLayoutRow{core::SettingsSection::language_server,
                                    server.action == application::LanguageServerRowAction::none
                                        ? core::SettingsControl::blank
                                        : core::SettingsControl::action});
    }
    if (view.servers_hidden > 0)
    {
        rows.push_back(core::SettingsLayoutRow{core::SettingsSection::language_server,
                                               core::SettingsControl::fold});
    }
    core::SettingsTextWidths widths{};
    for (std::size_t index = 0; index < view.tabs.size(); ++index)
    {
        widths.chips.at(index) = width(said(view.tabs.at(index).title), text_.label);
    }
    for (std::size_t index = 0; index < core::settings_presets.size(); ++index)
    {
        widths.presets.at(index) =
            width(said(preset_message(core::settings_presets.at(index))), text_.label);
    }
    widths.revert = width(said(core::UiMessage::settings_revert), text_.label);
    widths.close = width(said(core::UiMessage::settings_close), text_.label);
    widths.estimate = width(core::speed_estimate_text(view.summary, text_.language), text_.strong);
    return core::settings_layout(
        core::SettingsLayoutRequest{area, text_.dpi, rows, widths, view.scroll});
}

void SettingsPanelPainter::paint(const application::SettingsView &view,
                                 const core::SettingsLayout &layout, const core::Palette &palette,
                                 core::RgbColor heavy)
{
    palette_ = palette;
    heavy_ = heavy;
    fill(layout.panel, palette_.panel);
    fill(layout.border, palette_.panel_border);
    paint_header(layout);
    paint_search(view, layout);
    paint_chips(view, layout);
    fill(layout.divider, palette_.panel_border);
    target_.PushAxisAlignedClip(to_rect(layout.list), D2D1_ANTIALIAS_MODE_ALIASED);
    paint_items(view, layout);
    target_.PopAxisAlignedClip();
    paint_footer(view, layout);
}

void SettingsPanelPainter::paint_header(const core::SettingsLayout &layout)
{
    const auto used =
        write(said(core::UiMessage::settings_title), text_.title, layout.header, palette_.text);
    const auto chord =
        core::shown_chord(core::EditorOperation::open_settings, core::EditMode::ordinary);
    if (chord.has_value())
    {
        const auto label = core::key_chord_label(chord.value());
        const auto left = layout.header.left + used + pixels(box_padding_dips);
        const auto right = std::min(layout.header.right,
                                    left + width(label, text_.mono) + pixels(box_padding_dips + 4));
        KeycapStyle style = chord_style_shape;
        style.face = palette_.panel;
        style.border = palette_.panel_border;
        style.text = palette_.muted;
        paint_keycap(
            KeycapCanvas{target_, brush_, scaled(1.0F)}, line(label, text_.mono).Get(),
            core::LayoutRect{
                left, layout.header.top + ((core::height_of(layout.header) - line_height) / 2),
                right, layout.header.top + ((core::height_of(layout.header) + line_height) / 2)},
            style);
    }
    const auto centre = centre_of(layout.close);
    const float half = scaled(glyph_half_dips + 1.0F);
    stroke(D2D1::Point2F(centre.x - half, centre.y - half),
           D2D1::Point2F(centre.x + half, centre.y + half), palette_.muted);
    stroke(D2D1::Point2F(centre.x + half, centre.y - half),
           D2D1::Point2F(centre.x - half, centre.y + half), palette_.muted);
}

void SettingsPanelPainter::paint_search(const application::SettingsView &view,
                                        const core::SettingsLayout &layout)
{
    fill_rounded(layout.search, row_radius_dips, palette_.background);
    outline(layout.search, row_radius_dips, palette_.panel_border);
    // 虫めがね（円と柄）。
    const auto padding = pixels(summary_padding_dips);
    const core::LayoutRect icon{layout.search.left + padding, layout.search.top,
                                layout.search.left + padding + pixels(icon_dips),
                                layout.search.bottom};
    const auto centre = centre_of(icon);
    const float radius = scaled(glyph_half_dips + 0.5F);
    brush_.SetColor(to_color(palette_.muted));
    target_.DrawEllipse(
        D2D1::Ellipse(D2D1::Point2F(centre.x - scaled(1.0F), centre.y - scaled(1.0F)), radius,
                      radius),
        &brush_, scaled(glyph_stroke_dips));
    stroke(D2D1::Point2F(centre.x + scaled(2.5F), centre.y + scaled(2.5F)),
           D2D1::Point2F(centre.x + scaled(6.0F), centre.y + scaled(6.0F)), palette_.muted);
    const core::LayoutRect field{icon.right + pixels(tier_gap_dips), layout.search.top,
                                 layout.search.right - padding, layout.search.bottom};
    if (view.query.empty())
    {
        write(said(core::UiMessage::settings_search), text_.value, field, palette_.muted);
        return;
    }
    // 打った文字の後ろに 1 DIP の印（探す欄が文字の行き先だと分かる）。
    const auto used = std::min(write(view.query, text_.value, field, palette_.text),
                               core::width_of(field) - pixels(1));
    const auto middle = (field.top + field.bottom) / 2;
    fill(core::LayoutRect{field.left + used + pixels(1), middle - pixels(8),
                          field.left + used + pixels(2), middle + pixels(8)},
         palette_.text);
}

void SettingsPanelPainter::paint_chips(const application::SettingsView &view,
                                       const core::SettingsLayout &layout)
{
    for (std::size_t index = 0; index < view.tabs.size(); ++index)
    {
        const auto &tab = view.tabs.at(index);
        const auto &chip = layout.chips.at(index);
        const bool current = view.section == std::optional{tab.section};
        if (current)
        {
            fill_rounded(chip, chip_radius_dips, palette_.current_line);
        }
        outline(chip, chip_radius_dips, current ? palette_.accent : palette_.panel_border);
        // 絞って行の無い節の札は淡く描く。
        const auto ink = current ? palette_.text : (tab.filled ? palette_.muted : palette_.gutter);
        write_centered(said(tab.title), text_.label, chip, ink);
    }
}

void SettingsPanelPainter::paint_items(const application::SettingsView &view,
                                       const core::SettingsLayout &layout)
{
    for (const auto &item : layout.items)
    {
        // 行の領域に掛からないものは描かない（文字組みも作らない）。
        if (item.box.bottom > layout.list.top && item.box.top < layout.list.bottom)
        {
            paint_item(view, layout, item);
        }
    }
}

void SettingsPanelPainter::paint_item(const application::SettingsView &view,
                                      const core::SettingsLayout &layout,
                                      const core::SettingsLayoutItem &item)
{
    switch (item.kind)
    {
    case core::SettingsItemKind::heading:
        write(said(view.tabs.at(item.index).title), text_.heading,
              core::LayoutRect{item.box.left, item.box.top + pixels(6), item.box.right,
                               item.box.bottom - pixels(4)},
              palette_.muted);
        return;
    case core::SettingsItemKind::summary:
        paint_summary(view, layout, item.box);
        return;
    case core::SettingsItemKind::summary_note:
        paint_note(core::UiMessage::settings_tier_note, item.box);
        return;
    case core::SettingsItemKind::row:
        paint_any_row(view, layout, item);
        return;
    case core::SettingsItemKind::empty:
        write_centered(
            core::ui_text_filled(core::UiMessage::settings_no_match, text_.language, {view.query}),
            text_.note, item.box, palette_.muted);
        return;
    }
    std::unreachable();
}

void SettingsPanelPainter::paint_summary(const application::SettingsView &view,
                                         const core::SettingsLayout &layout,
                                         const core::LayoutRect &box)
{
    fill_rounded(box, row_radius_dips, palette_.background);
    outline(box, row_radius_dips, palette_.panel_border);
    const auto inside = inset(box, pixels(summary_padding_dips));
    const auto top = box.top + pixels(box_padding_dips);
    // 配置が箱を高くしていれば（1 行目が入り切らない）、1 行目を 2 行に折る。
    const auto extra = core::height_of(box) - pixels(core::settings_summary_height_dips);
    const auto middle = top + pixels(summary_line_dips) + std::max(extra, 0);
    const core::LayoutRect first{inside.left, top, inside.right, middle};
    if (extra > 0)
    {
        write_wrapped(core::speed_estimate_text(view.summary, text_.language), text_.strong, first,
                      palette_.text);
    }
    else
    {
        write(core::speed_estimate_text(view.summary, text_.language), text_.strong, first,
              palette_.text);
    }
    std::string names;
    for (const auto feature : view.summary.heavy)
    {
        names += names.empty() ? std::string_view{} : said(core::UiMessage::settings_list_joiner);
        names += said(core::settings_row_title(core::SettingsSubject{feature}));
    }
    const core::LayoutRect note{inside.left, middle + pixels(hint_gap_dips), inside.right,
                                middle + pixels(hint_gap_dips + summary_note_dips)};
    if (names.empty())
    {
        write(said(core::UiMessage::settings_summary_quiet), text_.note, note, palette_.muted);
    }
    else
    {
        write(
            core::ui_text_filled(core::UiMessage::settings_summary_heavy, text_.language, {names}),
            text_.note, note, heavy_);
    }
    for (std::size_t index = 0; index < layout.presets.size(); ++index)
    {
        const bool chosen = view.summary.preset == std::optional{core::settings_presets.at(index)};
        paint_button(said(preset_message(core::settings_presets.at(index))),
                     layout.presets.at(index), chosen, chosen ? palette_.on_accent : palette_.text);
    }
}

void SettingsPanelPainter::paint_row(const application::SettingsRowView &row,
                                     const core::SettingsLayout &layout,
                                     const core::SettingsLayoutItem &item)
{
    const auto control = core::settings_control_box(layout, item);
    paint_control(row, layout, item, control);
    const auto gap = pixels(core::settings_row_gap_dips);
    const auto right = control.left - gap;
    const auto left = item.box.left + pixels(core::settings_row_padding_dips);
    const core::LayoutRect title{left, item.box.top + pixels(title_top_dips), right,
                                 item.box.top + pixels(title_top_dips + title_height_dips)};
    // `:set` の名前は名前の行の右端（説明は下の行を部品の手前まで使う）。細い窓（全幅）では
    // 描かない。
    const auto named = layout.narrow ? 0 : width(row.set_name, text_.mono) + gap;
    if (!layout.narrow)
    {
        write_trailing(row.set_name, text_.mono, title, palette_.gutter);
    }
    const core::LayoutRect name{left, title.top, right - named, title.bottom};
    const auto used = write(said(row.title), text_.strong, name, palette_.text);
    paint_tier(row, core::LayoutRect{name.left + used + pixels(tier_gap_dips), name.top, name.right,
                                     name.bottom});
    write(said(row.description), text_.note,
          core::LayoutRect{left, title.bottom + pixels(hint_gap_dips), right,
                           item.box.bottom - pixels(title_top_dips)},
          palette_.muted);
}

void SettingsPanelPainter::paint_tier(const application::SettingsRowView &row,
                                      const core::LayoutRect &area)
{
    if (!row.tier.has_value() || core::width_of(area) <= 0)
    {
        return;
    }
    const auto tier = row.tier.value();
    const auto dots = static_cast<std::int32_t>(std::to_underlying(tier));
    const auto ink = tier == core::CostTier::high
                         ? heavy_
                         : (tier == core::CostTier::medium ? palette_.accent : palette_.muted);
    brush_.SetColor(to_color(ink));
    target_.PushAxisAlignedClip(to_rect(area), D2D1_ANTIALIAS_MODE_ALIASED);
    const float middle = static_cast<float>(area.top + area.bottom) / 2.0F;
    for (std::int32_t dot = 0; dot < dots; ++dot)
    {
        const float x = static_cast<float>(area.left) + scaled(dot_radius_dips) +
                        (scaled(dot_step_dips) * static_cast<float>(dot));
        target_.FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, middle), scaled(dot_radius_dips),
                                          scaled(dot_radius_dips)),
                            &brush_);
    }
    target_.PopAxisAlignedClip();
    std::string label{said(core::cost_tier_title(tier))};
    if (row.background)
    {
        label += said(core::UiMessage::settings_list_joiner);
        label += said(core::UiMessage::settings_tier_background);
    }
    const auto offset =
        static_cast<std::int32_t>(std::ceil(scaled(dot_step_dips) * static_cast<float>(dots)));
    write(label, text_.heading,
          core::LayoutRect{area.left + offset, area.top, area.right, area.bottom}, ink);
}

void SettingsPanelPainter::paint_control(const application::SettingsRowView &row,
                                         const core::SettingsLayout &layout,
                                         const core::SettingsLayoutItem &item,
                                         const core::LayoutRect &box)
{
    switch (item.control)
    {
    case core::SettingsControl::toggle:
        paint_switch(box, row.on);
        return;
    case core::SettingsControl::fixed:
        write_trailing(said(core::UiMessage::settings_always_on), text_.note, box, palette_.muted);
        return;
    case core::SettingsControl::choice:
        paint_value(row.value, box, true);
        return;
    case core::SettingsControl::stepper:
        paint_stepper(row.value, layout, box);
        return;
    case core::SettingsControl::readonly:
        paint_value(row.value, box, false);
        return;
    // 「言語サーバー」の節の部品は設定の行には来ない（paint_server と paint_fold が描く）。
    case core::SettingsControl::action:
    case core::SettingsControl::blank:
    case core::SettingsControl::fold:
        return;
    }
    std::unreachable();
}

void SettingsPanelPainter::paint_switch(const core::LayoutRect &box, bool on)
{
    fill_rounded(box, switch_radius_dips, on ? palette_.accent : palette_.toggle);
    const float reach = scaled(knob_inset_dips + knob_radius_dips);
    const float x =
        on ? static_cast<float>(box.right) - reach : static_cast<float>(box.left) + reach;
    brush_.SetColor(to_color(palette_.on_accent));
    target_.FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, centre_of(box).y), scaled(knob_radius_dips),
                                      scaled(knob_radius_dips)),
                        &brush_);
}

// 選ぶ欄（chosen・地と下向きの印）と読むだけの欄（枠だけ・字は muted）。
void SettingsPanelPainter::paint_value(std::string_view value, const core::LayoutRect &box,
                                       bool chosen)
{
    if (chosen)
    {
        fill_rounded(box, box_radius_dips, palette_.background);
    }
    outline(box, box_radius_dips, palette_.panel_border);
    const auto padding = pixels(box_padding_dips);
    const auto mark = chosen ? pixels(icon_dips) : 0;
    write(value, text_.value,
          core::LayoutRect{box.left + padding, box.top, box.right - padding - mark, box.bottom},
          chosen ? palette_.text : palette_.muted);
    if (!chosen)
    {
        return;
    }
    const float x = static_cast<float>(box.right - padding) - scaled(glyph_half_dips);
    const float y = centre_of(box).y;
    stroke(D2D1::Point2F(x - scaled(glyph_half_dips), y - scaled(2.0F)),
           D2D1::Point2F(x, y + scaled(2.0F)), palette_.muted);
    stroke(D2D1::Point2F(x, y + scaled(2.0F)),
           D2D1::Point2F(x + scaled(glyph_half_dips), y - scaled(2.0F)), palette_.muted);
}

void SettingsPanelPainter::paint_stepper(std::string_view value, const core::SettingsLayout &layout,
                                         const core::LayoutRect &box)
{
    fill_rounded(box, box_radius_dips, palette_.background);
    outline(box, box_radius_dips, palette_.panel_border);
    const float half = scaled(glyph_half_dips);
    const auto minus = centre_of(core::settings_step_box(layout, box, false));
    stroke(D2D1::Point2F(minus.x - half, minus.y), D2D1::Point2F(minus.x + half, minus.y),
           palette_.text);
    const auto plus = centre_of(core::settings_step_box(layout, box, true));
    stroke(D2D1::Point2F(plus.x - half, plus.y), D2D1::Point2F(plus.x + half, plus.y),
           palette_.text);
    stroke(D2D1::Point2F(plus.x, plus.y - half), D2D1::Point2F(plus.x, plus.y + half),
           palette_.text);
    write_centered(value, text_.label, box, palette_.text);
}

// 行の領域の 1 行（見えている行の通しの位置: 設定の行 → 言語サーバーの行 → 畳みの 1 行）。焦点の
// 行はどの行も current_line の面 ＋ text の 1 DIP の枠（#1929: current_line と panel の差は色では
// 見分けられない・text と panel の比は全テーマで 3:1 以上を契約が縛る・#1945 の形を全部の行へ。
// accent は背景に近いテーマがあるので枠に使わない・レビュー M1）。
void SettingsPanelPainter::paint_any_row(const application::SettingsView &view,
                                         const core::SettingsLayout &layout,
                                         const core::SettingsLayoutItem &item)
{
    if (view.focused == std::optional{item.index})
    {
        fill_rounded(item.box, row_radius_dips, palette_.current_line);
        outline(item.box, row_radius_dips, palette_.text);
    }
    if (item.index < view.rows.size())
    {
        paint_row(view.rows.at(item.index), layout, item);
        return;
    }
    const std::size_t server = item.index - view.rows.size();
    if (server < view.servers.size())
    {
        paint_server(view.servers.at(server), layout, item);
        return;
    }
    paint_fold(view, item.box);
}

// 状態の語と色（状態を色分けしない・語で伝え、動いている物の語は ● で始まる・レビュー R4）。
std::pair<core::UiMessage, core::RgbColor>
SettingsPanelPainter::server_state(application::LanguageServerRowState state) const noexcept
{
    switch (state)
    {
    case application::LanguageServerRowState::running:
        return {core::UiMessage::settings_lsp_state_running, palette_.text};
    case application::LanguageServerRowState::starting:
        return {core::UiMessage::settings_lsp_state_starting, palette_.muted};
    case application::LanguageServerRowState::stopped:
        return {core::UiMessage::settings_lsp_state_stopped, heavy_};
    case application::LanguageServerRowState::configured:
        return {core::UiMessage::settings_lsp_state_configured, palette_.muted};
    case application::LanguageServerRowState::found:
        return {core::UiMessage::settings_lsp_state_found, palette_.text};
    case application::LanguageServerRowState::missing:
        return {core::UiMessage::settings_lsp_state_missing, palette_.gutter};
    case application::LanguageServerRowState::searching:
        return {core::UiMessage::settings_lsp_state_searching, palette_.gutter};
    }
    std::unreachable();
}

// 言語サーバーの 1 行（1 行目: 言語の名前・サーバーの名前（等幅で小さく）・右端に状態の語、2 行目:
// パスか命令の行（等幅・入り切らなければパスは先頭側を「…」で省く）か案内の文、右: 「使う」か
// 「外す」）。
void SettingsPanelPainter::paint_server(const application::LanguageServerRowView &row,
                                        const core::SettingsLayout &layout,
                                        const core::SettingsLayoutItem &item)
{
    const auto control = core::settings_control_box(layout, item);
    paint_server_button(row.action, control);
    const auto gap = pixels(core::settings_row_gap_dips);
    const auto right = control.left - (core::width_of(control) > 0 ? gap : 0);
    const auto left = item.box.left + pixels(core::settings_row_padding_dips);
    const core::LayoutRect title{left, item.box.top + pixels(title_top_dips), right,
                                 item.box.top + pixels(title_top_dips + title_height_dips)};
    const auto [message, ink] = server_state(row.state);
    const auto state = said(message);
    const auto stated = std::min(width(state, text_.note), core::width_of(title));
    write_trailing(state, text_.note, title, ink);
    const core::LayoutRect name{left, title.top, title.right - stated - pixels(tier_gap_dips),
                                title.bottom};
    const auto used = write(row.title, text_.strong, name, palette_.text);
    write(row.server, text_.mono,
          core::LayoutRect{name.left + used + pixels(tier_gap_dips), name.top, name.right,
                           name.bottom},
          palette_.gutter);
    const core::LayoutRect second{left, title.bottom + pixels(hint_gap_dips), right,
                                  item.box.bottom - pixels(title_top_dips)};
    if (row.code && tail_matters(row.state))
    {
        write_tail(row.detail, text_.mono, second, palette_.muted);
        return;
    }
    write(row.detail, row.code ? text_.mono : text_.note, second, palette_.muted);
}

// 言語サーバーの行の右の押す物。「使う」は主ボタン（accent の地と on_accent の字・レビュー M1）、
// 「外す」は background の地と panel_border の枠と text の字、押す物の無い行は描かない。
void SettingsPanelPainter::paint_server_button(application::LanguageServerRowAction action,
                                               const core::LayoutRect &box)
{
    switch (action)
    {
    case application::LanguageServerRowAction::none:
        return;
    case application::LanguageServerRowAction::use:
        paint_button(said(core::UiMessage::settings_lsp_use), box, true, palette_.on_accent);
        return;
    case application::LanguageServerRowAction::remove:
        fill_rounded(box, box_radius_dips, palette_.background);
        paint_button(said(core::UiMessage::settings_lsp_remove), box, false, palette_.text);
        return;
    }
    std::unreachable();
}

// 2 行目が見つかったパスか命令の行（末尾の実行ファイルの名前が肝心）の状態か。入れ方の命令と探して
// いる間は頭が肝心なので末尾を切る。
bool SettingsPanelPainter::tail_matters(application::LanguageServerRowState state) noexcept
{
    switch (state)
    {
    case application::LanguageServerRowState::running:
    case application::LanguageServerRowState::starting:
    case application::LanguageServerRowState::configured:
    case application::LanguageServerRowState::found:
        return true;
    case application::LanguageServerRowState::stopped:
    case application::LanguageServerRowState::missing:
    case application::LanguageServerRowState::searching:
        return false;
    }
    std::unreachable();
}

// 入り切らなければ先頭側を「…」で省き、末尾を残して描く（パスの実行ファイルの名前を消さない・
// レビューの軽い指摘）。省く位置は全文の文字組みの当たり判定で決め、字の途中で切らない。
void SettingsPanelPainter::write_tail(std::string_view text, IDWriteTextFormat *format,
                                      const core::LayoutRect &area, core::RgbColor color)
{
    constexpr std::string_view ellipsis = "…";
    const auto full = width(text, format);
    const auto room = core::width_of(area);
    const auto shown = text.empty() ? nullptr : line(text, format);
    if (full <= room || shown == nullptr)
    {
        write(text, format, area, color);
        return;
    }
    BOOL trailing = FALSE;
    BOOL inside = FALSE;
    DWRITE_HIT_TEST_METRICS hit{};
    // 幅は切り上げなので 1 画素の余りを見込む。
    const auto cut = static_cast<float>(full - room + width(ellipsis, format) + 1);
    const std::wstring wide = core::to_utf16(text).value_or(std::wstring{});
    if (FAILED(shown->HitTestPoint(cut, 0.0F, &trailing, &inside, &hit)))
    {
        write(text, format, area, color);
        return;
    }
    // 当たった字が cut より左から始まるなら、その字ごと省く。
    const std::size_t from =
        std::min<std::size_t>(wide.size(), hit.textPosition + (hit.left < cut ? hit.length : 0U));
    std::string kept{ellipsis};
    kept += core::to_utf8(std::wstring_view{wide}.substr(from)).value_or(std::string{});
    write(kept, format, area, color);
}

// 「ほかの N 言語を見る」と下向きの印（行のどこを押しても開く）。
void SettingsPanelPainter::paint_fold(const application::SettingsView &view,
                                      const core::LayoutRect &box)
{
    const auto padding = pixels(core::settings_row_padding_dips);
    const auto text = core::ui_text_filled(core::UiMessage::settings_lsp_more, text_.language,
                                           {std::to_string(view.servers_hidden)});
    const auto used = write(text, text_.note,
                            core::LayoutRect{box.left + padding, box.top,
                                             box.right - padding - pixels(icon_dips), box.bottom},
                            palette_.text);
    const float x = static_cast<float>(box.left + padding + used) + scaled(glyph_half_dips * 2.0F);
    const float y = centre_of(box).y;
    stroke(D2D1::Point2F(x - scaled(glyph_half_dips), y - scaled(2.0F)),
           D2D1::Point2F(x, y + scaled(2.0F)), palette_.muted);
    stroke(D2D1::Point2F(x, y + scaled(2.0F)),
           D2D1::Point2F(x + scaled(glyph_half_dips), y - scaled(2.0F)), palette_.muted);
}

// muted の小さな 1 行（要約の箱の下の段の決め方）。
void SettingsPanelPainter::paint_note(core::UiMessage message, const core::LayoutRect &box)
{
    write(said(message), text_.note, box, palette_.muted);
}

void SettingsPanelPainter::paint_footer(const application::SettingsView &view,
                                        const core::SettingsLayout &layout)
{
    fill(layout.footer_line, palette_.panel_border);
    write(said(core::UiMessage::settings_saved_on_close), text_.note, layout.note, palette_.muted);
    // 「元に戻す」は開いたときから変えていなければ淡く描く（押しても何も変わらない）。
    paint_button(said(core::UiMessage::settings_revert), layout.revert, false,
                 view.changed ? palette_.text : palette_.muted);
    paint_button(said(core::UiMessage::settings_close), layout.close_button, true,
                 palette_.on_accent);
}

// filled は accent の地（閉じる・今の組）、そうでなければ枠だけ。
void SettingsPanelPainter::paint_button(std::string_view label, const core::LayoutRect &box,
                                        bool filled, core::RgbColor ink)
{
    if (filled)
    {
        fill_rounded(box, box_radius_dips, palette_.accent);
    }
    outline(box, box_radius_dips, filled ? palette_.accent : palette_.panel_border);
    write_centered(label, text_.label, box, ink);
}

PaletteRowLayouts::TextLayout SettingsPanelPainter::line(std::string_view text,
                                                         IDWriteTextFormat *format) const
{
    return composed(text, format, core::LayoutRect{0, 0, line_width, line_height}, false);
}

PaletteRowLayouts::TextLayout SettingsPanelPainter::wrapped(std::string_view text,
                                                            IDWriteTextFormat *format,
                                                            std::int32_t limit) const
{
    return composed(text, format, core::LayoutRect{0, 0, limit, line_height * 2}, true);
}

PaletteRowLayouts::TextLayout SettingsPanelPainter::trimmed(std::string_view text,
                                                            IDWriteTextFormat *format,
                                                            std::int32_t limit) const
{
    return composed(text, format, core::LayoutRect{0, 0, limit, line_height}, true);
}

// 箱 area の文字組み。trim なら入り切らない末尾を「…」で切り、2 行ぶんの高さの箱なら幅で折り返す
// （箱の幅と高さと役が保持の鍵に入るので、切った・折った文字組みと広い箱の文字組みは別の entry）。
PaletteRowLayouts::TextLayout SettingsPanelPainter::composed(std::string_view text,
                                                             IDWriteTextFormat *format,
                                                             const core::LayoutRect &area,
                                                             bool trim) const
{
    const bool folds = core::height_of(area) > line_height;
    return layouts_.obtain(
        PaletteRowKey{format, trim ? PaletteTextRole::trimmed : PaletteTextRole::plain, text, area,
                      text_.dpi},
        [&]() -> PaletteRowLayouts::TextLayout
        {
            const std::wstring wide = core::to_utf16(text).value_or(std::wstring{});
            PaletteRowLayouts::TextLayout made;
            if (text_.dwrite == nullptr || format == nullptr ||
                FAILED(text_.dwrite->CreateTextLayout(
                    wide.c_str(), static_cast<UINT32>(wide.size()), format,
                    static_cast<float>(core::width_of(area)),
                    static_cast<float>(core::height_of(area)), made.GetAddressOf())) ||
                (folds && FAILED(made->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP))))
            {
                return nullptr;
            }
            Microsoft::WRL::ComPtr<IDWriteInlineObject> sign;
            const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
            if (trim &&
                (FAILED(text_.dwrite->CreateEllipsisTrimmingSign(format, sign.GetAddressOf())) ||
                 FAILED(made->SetTrimming(&trimming, sign.Get()))))
            {
                return nullptr;
            }
            return made;
        });
}

std::int32_t SettingsPanelPainter::width(std::string_view text, IDWriteTextFormat *format) const
{
    const auto shown = text.empty() ? nullptr : line(text, format);
    DWRITE_TEXT_METRICS metrics{};
    if (shown == nullptr || FAILED(shown->GetMetrics(&metrics)))
    {
        return 0;
    }
    return static_cast<std::int32_t>(std::ceil(metrics.widthIncludingTrailingWhitespace));
}

std::int32_t SettingsPanelPainter::write(std::string_view text, IDWriteTextFormat *format,
                                         const core::LayoutRect &area, core::RgbColor color)
{
    if (text.empty() || core::width_of(area) <= 0 || core::height_of(area) <= 0)
    {
        return 0;
    }
    // 入り切らない文は末尾を「…」で切った文字組みで描く（字の途中で切らない・レビュー R3）。
    const auto full = width(text, format);
    const auto room = core::width_of(area);
    const auto shown = full > room ? trimmed(text, format, room) : line(text, format);
    if (shown == nullptr)
    {
        return 0;
    }
    brush_.SetColor(to_color(color));
    target_.PushAxisAlignedClip(to_rect(area), D2D1_ANTIALIAS_MODE_ALIASED);
    const float top = static_cast<float>(area.top) +
                      (static_cast<float>(core::height_of(area) - line_height) / 2.0F);
    target_.DrawTextLayout(D2D1::Point2F(static_cast<float>(area.left), top), shown.Get(), &brush_);
    target_.PopAxisAlignedClip();
    return std::min(full, room);
}

void SettingsPanelPainter::write_wrapped(std::string_view text, IDWriteTextFormat *format,
                                         const core::LayoutRect &area, core::RgbColor color)
{
    const auto shown = text.empty() ? nullptr : wrapped(text, format, core::width_of(area));
    if (shown == nullptr || core::width_of(area) <= 0 || core::height_of(area) <= 0)
    {
        return;
    }
    brush_.SetColor(to_color(color));
    target_.PushAxisAlignedClip(to_rect(area), D2D1_ANTIALIAS_MODE_ALIASED);
    const float top = static_cast<float>(area.top) +
                      (static_cast<float>(core::height_of(area) - (line_height * 2)) / 2.0F);
    target_.DrawTextLayout(D2D1::Point2F(static_cast<float>(area.left), top), shown.Get(), &brush_);
    target_.PopAxisAlignedClip();
}

void SettingsPanelPainter::write_centered(std::string_view text, IDWriteTextFormat *format,
                                          const core::LayoutRect &area, core::RgbColor color)
{
    const auto spare = std::max(0, core::width_of(area) - width(text, format));
    write(text, format,
          core::LayoutRect{area.left + (spare / 2), area.top, area.right, area.bottom}, color);
}

void SettingsPanelPainter::write_trailing(std::string_view text, IDWriteTextFormat *format,
                                          const core::LayoutRect &area, core::RgbColor color)
{
    const auto spare = std::max(0, core::width_of(area) - width(text, format));
    write(text, format, core::LayoutRect{area.left + spare, area.top, area.right, area.bottom},
          color);
}

void SettingsPanelPainter::fill(const core::LayoutRect &area, core::RgbColor color)
{
    brush_.SetColor(to_color(color));
    target_.FillRectangle(to_rect(area), &brush_);
}

void SettingsPanelPainter::fill_rounded(const core::LayoutRect &area, float radius_dips,
                                        core::RgbColor color)
{
    brush_.SetColor(to_color(color));
    target_.FillRoundedRectangle(
        D2D1::RoundedRect(to_rect(area), scaled(radius_dips), scaled(radius_dips)), &brush_);
}

// 1 DIP の枠（線の中心を半分だけ内側に置き、矩形の中に収める）。
void SettingsPanelPainter::outline(const core::LayoutRect &area, float radius_dips,
                                   core::RgbColor color)
{
    const float half = scaled(1.0F) / 2.0F;
    auto box = to_rect(area);
    box.left += half;
    box.top += half;
    box.right -= half;
    box.bottom -= half;
    brush_.SetColor(to_color(color));
    target_.DrawRoundedRectangle(D2D1::RoundedRect(box, scaled(radius_dips), scaled(radius_dips)),
                                 &brush_, scaled(1.0F));
}

void SettingsPanelPainter::stroke(D2D1_POINT_2F from, D2D1_POINT_2F to, core::RgbColor color)
{
    brush_.SetColor(to_color(color));
    target_.DrawLine(from, to, &brush_, scaled(glyph_stroke_dips));
}
} // namespace nenenib::ui::win32
