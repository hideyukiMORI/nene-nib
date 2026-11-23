// 設定の面の項目の列の読み口（ADR 0170 の決定 5・2026-11-18 の追補 2・#1016 の段 1）。
#include "SettingsRows.hpp"

#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "CaretGlide.hpp"
#include "DisplayLanguage.hpp"
#include "DisplayLanguageChoice.hpp"
#include "DisplayLanguageNames.hpp"
#include "EditorFeature.hpp"
#include "EditorSettings.hpp"
#include "FeatureCosts.hpp"
#include "FinderPreview.hpp"
#include "FontSize.hpp"
#include "FontSizeAdjustment.hpp"
#include "IndentGuide.hpp"
#include "IndentGuideStyle.hpp"
#include "PaneResizeStep.hpp"
#include "SettingsRow.hpp"
#include "SettingsSection.hpp"
#include "SettingsSubject.hpp"
#include "SettingsValue.hpp"
#include "ThemeChoice.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
// 置き方の送りの順（宣言の順）。
constexpr std::array<FinderPreview, 4> finder_preview_order{
    FinderPreview::automatic, FinderPreview::right, FinderPreview::below, FinderPreview::off};

// プレビューの置き方の表示の名前。
[[nodiscard]] UiMessage finder_preview_title(FinderPreview preview) noexcept
{
    switch (preview)
    {
    case FinderPreview::automatic:
        return UiMessage::settings_finder_preview_automatic;
    case FinderPreview::right:
        return UiMessage::settings_finder_preview_right;
    case FinderPreview::below:
        return UiMessage::settings_finder_preview_below;
    case FinderPreview::off:
        return UiMessage::settings_finder_preview_off;
    }
    std::unreachable();
}

// 単色 / 虹の送りの順（宣言の順）と表示の名前。
constexpr std::array<IndentGuideStyle, 2> indent_guide_style_order{IndentGuideStyle::rainbow,
                                                                   IndentGuideStyle::plain};

[[nodiscard]] UiMessage indent_guide_style_title(IndentGuideStyle style) noexcept
{
    switch (style)
    {
    case IndentGuideStyle::rainbow:
        return UiMessage::settings_indent_guide_style_rainbow;
    case IndentGuideStyle::plain:
        return UiMessage::settings_indent_guide_style_plain;
    }
    std::unreachable();
}

[[nodiscard]] UiMessage value_title(SettingsValue value) noexcept
{
    switch (value)
    {
    case SettingsValue::theme:
        return UiMessage::settings_theme_name;
    case SettingsValue::ui_language:
        return UiMessage::settings_language_name;
    case SettingsValue::font_family:
        return UiMessage::settings_font_family_name;
    case SettingsValue::font_size:
        return UiMessage::settings_font_size_name;
    case SettingsValue::finder_preview:
        return UiMessage::settings_finder_preview_name;
    case SettingsValue::caret_motion_time:
        return UiMessage::settings_caret_motion_time_name;
    case SettingsValue::indent_guide_style:
        return UiMessage::settings_indent_guide_style_name;
    case SettingsValue::indent_guide_colors:
        return UiMessage::settings_indent_guide_colors_name;
    case SettingsValue::indent_guide_active:
        return UiMessage::settings_indent_guide_active_name;
    case SettingsValue::indent_guide_width:
        return UiMessage::settings_indent_guide_width_name;
    case SettingsValue::pane_resize_step:
        return UiMessage::settings_pane_resize_step_name;
    case SettingsValue::pane_resize_step_vim:
        return UiMessage::settings_pane_resize_step_vim_name;
    }
    std::unreachable();
}

[[nodiscard]] UiMessage value_description(SettingsValue value) noexcept
{
    switch (value)
    {
    case SettingsValue::theme:
        return UiMessage::settings_theme_description;
    case SettingsValue::ui_language:
        return UiMessage::settings_language_description;
    case SettingsValue::font_family:
        return UiMessage::settings_font_family_description;
    case SettingsValue::font_size:
        return UiMessage::settings_font_size_description;
    case SettingsValue::finder_preview:
        return UiMessage::settings_finder_preview_description;
    case SettingsValue::caret_motion_time:
        return UiMessage::settings_caret_motion_time_description;
    case SettingsValue::indent_guide_style:
        return UiMessage::settings_indent_guide_style_description;
    case SettingsValue::indent_guide_colors:
        return UiMessage::settings_indent_guide_colors_description;
    case SettingsValue::indent_guide_active:
        return UiMessage::settings_indent_guide_active_description;
    case SettingsValue::indent_guide_width:
        return UiMessage::settings_indent_guide_width_description;
    case SettingsValue::pane_resize_step:
        return UiMessage::settings_pane_resize_step_description;
    case SettingsValue::pane_resize_step_vim:
        return UiMessage::settings_pane_resize_step_vim_description;
    }
    std::unreachable();
}

[[nodiscard]] std::string_view value_set_name(SettingsValue value) noexcept
{
    switch (value)
    {
    case SettingsValue::theme:
        return "colorscheme";
    case SettingsValue::ui_language:
        return "language";
    case SettingsValue::font_family:
        return "guifont";
    case SettingsValue::font_size:
        return "fontsize";
    case SettingsValue::finder_preview:
        return "finderpreview";
    case SettingsValue::caret_motion_time:
        return "caretmotiontime";
    case SettingsValue::indent_guide_style:
        return "indentguidestyle";
    case SettingsValue::indent_guide_colors:
        return "indentguidecolors";
    case SettingsValue::indent_guide_active:
        return "indentguideactive";
    case SettingsValue::indent_guide_width:
        return "indentguidewidth";
    case SettingsValue::pane_resize_step:
        return "paneresizestep";
    case SettingsValue::pane_resize_step_vim:
        return "paneresizestepvim";
    }
    std::unreachable();
}

[[nodiscard]] char folded(char byte) noexcept
{
    return byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte - 'A' + 'a') : byte;
}

// text が needle を含むか（ASCII の大小を区別しない）。部分列の探索の STL（__std_search_*）を core
// の外へ出さないよう 1 バイトずつ比べる（ARC-003・first_slot_split と同じ理由）。
[[nodiscard]] bool contains_folded(std::string_view text, std::string_view needle) noexcept
{
    if (needle.size() > text.size())
    {
        return false;
    }
    for (std::size_t start = 0; start + needle.size() <= text.size(); ++start)
    {
        std::size_t matched = 0;
        while (matched < needle.size() &&
               folded(text.at(start + matched)) == folded(needle.at(matched)))
        {
            ++matched;
        }
        if (matched == needle.size())
        {
            return true;
        }
    }
    return false;
}

// 列 order の中の current の次（前）の値。列に無ければ先頭。
template <typename Value, std::size_t Count>
[[nodiscard]] Value cycled(const std::array<Value, Count> &order, Value current,
                           bool forward) noexcept
{
    for (std::size_t index = 0; index < Count; ++index)
    {
        if (order.at(index) == current)
        {
            return order.at(forward ? (index + 1) % Count : (index + Count - 1) % Count);
        }
    }
    return order.front();
}

// 組み込みの一覧に無い配色（利用者のテーマ）か。
[[nodiscard]] bool outside_builtins(const std::optional<ThemeChoice> &theme) noexcept
{
    return theme.has_value() && !theme_named(theme.value().name()).has_value();
}

// 配色の送り。列の 0 は system、1 からが組み込みの表の添字 + 1、最後が一覧に無い配色（あれば）。
[[nodiscard]] std::optional<ThemeChoice> theme_stepped(const std::optional<ThemeChoice> &current,
                                                       bool forward,
                                                       const std::optional<ThemeChoice> &kept)
{
    const std::optional<ThemeChoice> extra = outside_builtins(current) ? current
                                             : outside_builtins(kept)  ? kept
                                                                       : std::nullopt;
    const std::size_t builtins = builtin_themes.size();
    const std::size_t count = builtins + 1 + (extra.has_value() ? 1 : 0);
    std::size_t at = 0;
    if (current.has_value())
    {
        const auto builtin = theme_named(current.value().name());
        at = builtin.has_value() ? static_cast<std::size_t>(builtin.value()) + 1 : builtins + 1;
    }
    const std::size_t next = forward ? (at + 1) % count : (at + count - 1) % count;
    if (next == 0)
    {
        return std::nullopt;
    }
    if (next == builtins + 1)
    {
        return extra;
    }
    return ThemeChoice::from(static_cast<BuiltinTheme>(next - 1));
}
} // namespace

UiMessage settings_section_title(SettingsSection section) noexcept
{
    switch (section)
    {
    case SettingsSection::display:
        return UiMessage::settings_section_display;
    case SettingsSection::text:
        return UiMessage::settings_section_text;
    case SettingsSection::editing:
        return UiMessage::settings_section_editing;
    case SettingsSection::panes:
        return UiMessage::settings_section_panes;
    case SettingsSection::features:
        return UiMessage::settings_section_features;
    case SettingsSection::language_server:
        return UiMessage::settings_section_language_server;
    }
    std::unreachable();
}

UiMessage settings_row_title(const SettingsSubject &subject) noexcept
{
    return std::visit(
        [](auto value) noexcept
        {
            if constexpr (std::is_same_v<decltype(value), EditorFeature>)
            {
                return feature_cost(value).title;
            }
            else
            {
                return value_title(value);
            }
        },
        subject);
}

UiMessage settings_row_description(const SettingsSubject &subject) noexcept
{
    return std::visit(
        [](auto value) noexcept
        {
            if constexpr (std::is_same_v<decltype(value), EditorFeature>)
            {
                return feature_cost(value).description;
            }
            else
            {
                return value_description(value);
            }
        },
        subject);
}

std::string_view settings_row_set_name(const SettingsSubject &subject) noexcept
{
    return std::visit(
        [](auto value) noexcept
        {
            if constexpr (std::is_same_v<decltype(value), EditorFeature>)
            {
                return feature_name_of(value);
            }
            else
            {
                return value_set_name(value);
            }
        },
        subject);
}

bool settings_row_matches(const SettingsRow &row, std::string_view query,
                          DisplayLanguage language) noexcept
{
    return query.empty() || contains_folded(settings_row_set_name(row.subject), query) ||
           contains_folded(ui_text(settings_row_title(row.subject), language), query) ||
           contains_folded(ui_text(settings_row_description(row.subject), language), query);
}

std::string settings_value_text(const EditorSettings &settings, SettingsValue value,
                                DisplayLanguage language)
{
    switch (value)
    {
    case SettingsValue::theme:
        return std::string(settings.theme.has_value()
                               ? settings.theme.value().name()
                               : ui_text(UiMessage::language_choice_system, language));
    case SettingsValue::ui_language:
        return std::string(display_language_choice_detail(settings.ui_language, language));
    case SettingsValue::font_family:
        return std::string(settings.font_family.text());
    case SettingsValue::font_size:
        return font_size_text(settings.font_size);
    case SettingsValue::finder_preview:
        return std::string(ui_text(finder_preview_title(settings.finder_preview), language));
    case SettingsValue::caret_motion_time:
        return caret_motion_time_text(settings.caret_motion_time);
    case SettingsValue::indent_guide_style:
        return std::string(
            ui_text(indent_guide_style_title(settings.indent_guide_style), language));
    case SettingsValue::indent_guide_colors:
        return std::to_string(settings.indent_guide_colors);
    case SettingsValue::indent_guide_active:
        return std::string(ui_text(settings.indent_guide_active
                                       ? UiMessage::settings_indent_guide_active_on
                                       : UiMessage::settings_indent_guide_active_off,
                                   language));
    case SettingsValue::indent_guide_width:
        return std::to_string(settings.indent_guide_width);
    case SettingsValue::pane_resize_step:
        return pane_resize_step_text(settings.pane_resize_step);
    case SettingsValue::pane_resize_step_vim:
        return std::string(ui_text(settings.pane_resize_step_vim
                                       ? UiMessage::settings_pane_resize_step_vim_follows
                                       : UiMessage::settings_pane_resize_step_vim_single,
                                   language));
    }
    std::unreachable();
}

EditorSettings settings_value_stepped(EditorSettings settings, SettingsValue value, bool forward,
                                      const std::optional<ThemeChoice> &kept)
{
    switch (value)
    {
    case SettingsValue::theme:
        settings.theme = theme_stepped(settings.theme, forward, kept);
        return settings;
    case SettingsValue::ui_language:
        settings.ui_language =
            cycled(listed_display_language_choices, settings.ui_language, forward);
        return settings;
    case SettingsValue::font_family:
        return settings;
    case SettingsValue::font_size:
        settings.font_size = adjusted_font_size(
            settings.font_size,
            forward ? FontSizeAdjustment::increase : FontSizeAdjustment::decrease, 1);
        return settings;
    case SettingsValue::finder_preview:
        settings.finder_preview = cycled(finder_preview_order, settings.finder_preview, forward);
        return settings;
    case SettingsValue::caret_motion_time:
        settings.caret_motion_time = caret_motion_time_stepped(settings.caret_motion_time, forward);
        return settings;
    case SettingsValue::indent_guide_style:
        settings.indent_guide_style =
            cycled(indent_guide_style_order, settings.indent_guide_style, forward);
        return settings;
    case SettingsValue::indent_guide_colors:
        settings.indent_guide_colors =
            indent_guide_colors_stepped(settings.indent_guide_colors, forward);
        return settings;
    case SettingsValue::indent_guide_active:
        settings.indent_guide_active = !settings.indent_guide_active;
        return settings;
    case SettingsValue::indent_guide_width:
        settings.indent_guide_width =
            indent_guide_width_stepped(settings.indent_guide_width, forward);
        return settings;
    case SettingsValue::pane_resize_step:
        settings.pane_resize_step = pane_resize_step_stepped(settings.pane_resize_step, forward);
        return settings;
    case SettingsValue::pane_resize_step_vim:
        // 2 つの選択肢なので、どちらの向きに送っても反対へ。
        settings.pane_resize_step_vim = !settings.pane_resize_step_vim;
        return settings;
    }
    std::unreachable();
}
} // namespace nenenib::core
