// 設定の面（右の引き出し）の描画と当たり（施主決定 D102・ADR 0170 の決定 5・#1836）。
// Direct2DRenderer.cpp の長さの上限（CNF-014）のため翻訳単位を分けた。
#include "Direct2DRenderer.hpp"

#include "DevicePixels.hpp"
#include "EditorFrame.hpp"
#include "LayoutRect.hpp"
#include "SettingsChoices.hpp"
#include "SettingsPanel.hpp"
#include "SettingsPanelPainter.hpp"
#include "SettingsPart.hpp"
#include "SettingsSection.hpp"
#include "UiLocale.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::ui::win32
{
core::SettingsTarget Direct2DRenderer::settings_target_at(std::int32_t x, std::int32_t y) const
{
    return settings_layout_.has_value() ? core::settings_hit(settings_layout_.value(), x, y)
                                        : core::SettingsTarget{core::SettingsPart::outside, 0};
}

const std::optional<core::SettingsLayout> &Direct2DRenderer::settings_layout() const noexcept
{
    return settings_layout_;
}

std::optional<std::int32_t> Direct2DRenderer::settings_scrolled(std::int32_t from,
                                                                std::int32_t lines) const
{
    if (!settings_layout_.has_value())
    {
        return std::nullopt;
    }
    const auto moved = core::settings_scroll_by(settings_layout_.value(), from, lines);
    return moved == from ? std::nullopt : std::optional{moved};
}

std::optional<core::LayoutRect> Direct2DRenderer::settings_search_rectangle() const
{
    return settings_layout_.transform([](const core::SettingsLayout &layout)
                                      { return layout.search; });
}

// 書式は面を最初に描くときに作る（開いていない間は持たない）。
bool Direct2DRenderer::settings_formats_made()
{
    return settings_formats_.made(
        *dwrite_.Get(),
        {family(L"Segoe UI Variable Text", L"Segoe UI"), family(L"Cascadia Code", L"Consolas")},
        static_cast<float>(dpi_) / static_cast<float>(core::reference_dpi),
        ui_locale_name(formatted_language_));
}

std::optional<std::int32_t>
Direct2DRenderer::settings_revealed(const application::SettingsView &view,
                                    core::DisplayLanguage language,
                                    std::optional<core::SettingsSection> section)
{
    // 面をまだ描いていなければ帯が分からない（開いた直後は焦点が最初の行なので送りは要らない）。
    const bool aimed = section.has_value() || view.focused.has_value();
    if (!settings_layout_.has_value() || !aimed || !settings_formats_made())
    {
        return std::nullopt;
    }
    const SettingsPanelPainter painter(*context_.Get(), *brush_.Get(),
                                       settings_formats_.text(dwrite_.Get(), dpi_, language),
                                       settings_layouts_);
    const auto layout = painter.arranged(view, settings_area_);
    const auto moved = section.has_value()
                           ? core::settings_scroll_revealing_section(layout, section.value())
                           : core::settings_scroll_revealing(layout, view.focused.value());
    return moved == view.scroll ? std::nullopt : std::optional{moved};
}

void Direct2DRenderer::draw_settings(const application::EditorFrame &frame,
                                     const core::LayoutRect &area)
{
    if (!frame.settings_panel.has_value())
    {
        // 閉じたら文字組みと配置を捨てる。開くたびに作り直す（右クリックのメニューと同じ）。
        if (settings_layout_.has_value())
        {
            settings_layouts_.clear();
            settings_layout_.reset();
        }
        settings_choice_layout_.reset();
        return;
    }
    if (!settings_formats_made())
    {
        return;
    }
    const auto &view = frame.settings_panel.value();
    SettingsPanelPainter painter(*context_.Get(), *brush_.Get(),
                                 settings_formats_.text(dwrite_.Get(), dpi_, frame.language),
                                 settings_layouts_);
    settings_area_ = area;
    settings_layout_ = painter.arranged(view, area);
    settings_choice_layout_ = painter.arranged_choice(view, settings_layout_.value());
    painter.paint(view, settings_layout_.value(), frame.palette, frame.body.error);
}

const std::optional<core::SettingsChoiceLayout> &
Direct2DRenderer::settings_choice_layout() const noexcept
{
    return settings_choice_layout_;
}

// 一覧の配置は view から作り直す（描く前に 2 通来ても今の値で寄せる・settings_revealed と同じ）。
std::optional<core::SettingsChoiceLayout>
Direct2DRenderer::settings_choice_arranged(const application::SettingsView &view,
                                           core::DisplayLanguage language)
{
    if (!settings_layout_.has_value() || !view.choice.has_value() || !settings_formats_made())
    {
        return std::nullopt;
    }
    const SettingsPanelPainter painter(*context_.Get(), *brush_.Get(),
                                       settings_formats_.text(dwrite_.Get(), dpi_, language),
                                       settings_layouts_);
    return painter.arranged_choice(view, painter.arranged(view, settings_area_));
}

std::optional<std::size_t>
Direct2DRenderer::settings_choice_revealed(const application::SettingsView &view,
                                           core::DisplayLanguage language)
{
    const auto layout = settings_choice_arranged(view, language);
    if (!layout.has_value() || !view.choice.has_value())
    {
        return std::nullopt;
    }
    const auto &choice = view.choice.value();
    const auto top = core::settings_choice_revealing(layout.value(), choice.candidate);
    return top == choice.top ? std::nullopt : std::optional{top};
}

std::optional<std::size_t>
Direct2DRenderer::settings_choice_scrolled(const application::SettingsView &view,
                                           core::DisplayLanguage language, std::int32_t lines)
{
    const auto layout = settings_choice_arranged(view, language);
    if (!layout.has_value() || lines == 0)
    {
        return std::nullopt;
    }
    const auto first = static_cast<std::int64_t>(layout.value().first);
    const auto last = static_cast<std::int64_t>(layout.value().count - layout.value().rows);
    const auto moved = std::clamp(first + lines, std::int64_t{0}, last);
    return moved == first ? std::nullopt : std::optional{static_cast<std::size_t>(moved)};
}
} // namespace nenenib::ui::win32
