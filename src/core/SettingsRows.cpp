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
#include "SettingsChoices.hpp"
#include "SettingsControl.hpp"
#include "SettingsPanel.hpp"
#include "SettingsRow.hpp"
#include "SettingsSection.hpp"
#include "SettingsSubject.hpp"
#include "SettingsValue.hpp"
#include "ThemeChoice.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

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

// 2 つの選択肢の値の列（選ぶ欄の一覧の順・#1926）。どちらの向きに送っても反対へ行くので、送りは
// この順に依らない。今の線を強めるは「強める → 強めない」、Vim の鍵は「1 回分 → 回数に従う」。
constexpr std::array<bool, 2> indent_guide_active_order{true, false};
constexpr std::array<bool, 2> pane_resize_step_vim_order{false, true};

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

// 配色の列: 0 は system、1 からが組み込みの表の添字 + 1、最後が一覧に無い配色（あれば）。送り
// （theme_stepped）と選ぶ欄の一覧（settings_choice_*・#1926）が同じこの列を読む。
// 列の最後の一覧に無い配色（今の値が利用者のテーマならそれ、でなければ kept が利用者のテーマなら
// それ・無ければ値なし）。
[[nodiscard]] std::optional<ThemeChoice> theme_extra(const std::optional<ThemeChoice> &current,
                                                     const std::optional<ThemeChoice> &kept)
{
    return outside_builtins(current) ? current : outside_builtins(kept) ? kept : std::nullopt;
}

[[nodiscard]] std::size_t theme_count(const std::optional<ThemeChoice> &current,
                                      const std::optional<ThemeChoice> &kept) noexcept
{
    return builtin_themes.size() + 1 +
           (outside_builtins(current) || outside_builtins(kept) ? 1 : 0);
}

[[nodiscard]] std::size_t theme_position(const std::optional<ThemeChoice> &current) noexcept
{
    if (!current.has_value())
    {
        return 0;
    }
    const auto builtin = theme_named(current.value().name());
    return builtin.has_value() ? static_cast<std::size_t>(builtin.value()) + 1
                               : builtin_themes.size() + 1;
}

[[nodiscard]] std::optional<ThemeChoice> theme_at(std::size_t position,
                                                  const std::optional<ThemeChoice> &extra)
{
    if (position == 0)
    {
        return std::nullopt;
    }
    if (position == builtin_themes.size() + 1)
    {
        return extra;
    }
    return ThemeChoice::from(static_cast<BuiltinTheme>(position - 1));
}

// 配色の送り（列の上で回り込む）。
[[nodiscard]] std::optional<ThemeChoice> theme_stepped(const std::optional<ThemeChoice> &current,
                                                       bool forward,
                                                       const std::optional<ThemeChoice> &kept)
{
    const std::size_t count = theme_count(current, kept);
    const std::size_t at = theme_position(current);
    return theme_at(forward ? (at + 1) % count : (at + count - 1) % count,
                    theme_extra(current, kept));
}

// 列 order の中の value の位置。列に無ければ値なし。
template <typename Value, std::size_t Count>
[[nodiscard]] std::optional<std::size_t> position_in(const std::array<Value, Count> &order,
                                                     Value value) noexcept
{
    for (std::size_t index = 0; index < Count; ++index)
    {
        if (order.at(index) == value)
        {
            return index;
        }
    }
    return std::nullopt;
}

// 列 order の位置 position の値（列の外は fallback）。
template <typename Value, std::size_t Count>
[[nodiscard]] Value value_at(const std::array<Value, Count> &order, std::size_t position,
                             Value fallback) noexcept
{
    return position < Count ? order.at(position) : fallback;
}

// 列 order の名前の列（named は値 → 表示の名前）。
template <typename Value, std::size_t Count, typename Named>
[[nodiscard]] std::vector<std::string> texts_of(const std::array<Value, Count> &order, Named named)
{
    std::vector<std::string> texts;
    texts.reserve(Count);
    for (const auto value : order)
    {
        texts.emplace_back(named(value));
    }
    return texts;
}

[[nodiscard]] UiMessage indent_guide_active_title(bool active) noexcept
{
    return active ? UiMessage::settings_indent_guide_active_on
                  : UiMessage::settings_indent_guide_active_off;
}

[[nodiscard]] UiMessage pane_resize_step_vim_title(bool follows) noexcept
{
    return follows ? UiMessage::settings_pane_resize_step_vim_follows
                   : UiMessage::settings_pane_resize_step_vim_single;
}

// 配色の名前の列（settings_value_text の配色の名前と同じ）。
[[nodiscard]] std::vector<std::string> theme_texts(const std::optional<ThemeChoice> &current,
                                                   const std::optional<ThemeChoice> &kept,
                                                   DisplayLanguage language)
{
    std::vector<std::string> texts;
    texts.reserve(theme_count(current, kept));
    texts.emplace_back(ui_text(UiMessage::language_choice_system, language));
    for (const auto &theme : builtin_themes)
    {
        texts.emplace_back(theme.name);
    }
    const auto extra = theme_extra(current, kept);
    if (extra.has_value())
    {
        texts.emplace_back(extra.value().name());
    }
    return texts;
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
        return std::string(
            ui_text(indent_guide_active_title(settings.indent_guide_active), language));
    case SettingsValue::indent_guide_width:
        return std::to_string(settings.indent_guide_width);
    case SettingsValue::pane_resize_step:
        return pane_resize_step_text(settings.pane_resize_step);
    case SettingsValue::pane_resize_step_vim:
        return std::string(
            ui_text(pane_resize_step_vim_title(settings.pane_resize_step_vim), language));
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
        settings.indent_guide_active =
            cycled(indent_guide_active_order, settings.indent_guide_active, forward);
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
        settings.pane_resize_step_vim =
            cycled(pane_resize_step_vim_order, settings.pane_resize_step_vim, forward);
        return settings;
    }
    std::unreachable();
}

std::size_t settings_choice_count(const EditorSettings &settings, SettingsValue value,
                                  const std::optional<ThemeChoice> &kept) noexcept
{
    // 「一覧を持つか」は面の部品の決め（settings_control_of の choice）の 1 本を読む（レビュー
    // M1）。choice の値の列が 2 未満なら試験 verify_choice_rows_have_lists が落ちる。
    if (settings_control_of(SettingsSubject{value}, false) != SettingsControl::choice)
    {
        return 0;
    }
    switch (value)
    {
    case SettingsValue::theme:
        return theme_count(settings.theme, kept);
    case SettingsValue::ui_language:
        return listed_display_language_choices.size();
    case SettingsValue::finder_preview:
        return finder_preview_order.size();
    case SettingsValue::indent_guide_style:
        return indent_guide_style_order.size();
    case SettingsValue::indent_guide_active:
        return indent_guide_active_order.size();
    case SettingsValue::pane_resize_step_vim:
        return pane_resize_step_vim_order.size();
    // 段の上下と読むだけの欄は一覧を持たない。
    case SettingsValue::font_family:
    case SettingsValue::font_size:
    case SettingsValue::caret_motion_time:
    case SettingsValue::indent_guide_colors:
    case SettingsValue::indent_guide_width:
    case SettingsValue::pane_resize_step:
        return 0;
    }
    std::unreachable();
}

std::optional<std::size_t> settings_choice_position(const EditorSettings &settings,
                                                    SettingsValue value,
                                                    const std::optional<ThemeChoice> &kept) noexcept
{
    if (settings_choice_count(settings, value, kept) == 0)
    {
        return std::nullopt;
    }
    switch (value)
    {
    case SettingsValue::theme:
        return std::min(theme_position(settings.theme), theme_count(settings.theme, kept) - 1);
    case SettingsValue::ui_language:
        return position_in(listed_display_language_choices, settings.ui_language);
    case SettingsValue::finder_preview:
        return position_in(finder_preview_order, settings.finder_preview);
    case SettingsValue::indent_guide_style:
        return position_in(indent_guide_style_order, settings.indent_guide_style);
    case SettingsValue::indent_guide_active:
        return position_in(indent_guide_active_order, settings.indent_guide_active);
    case SettingsValue::pane_resize_step_vim:
        return position_in(pane_resize_step_vim_order, settings.pane_resize_step_vim);
    case SettingsValue::font_family:
    case SettingsValue::font_size:
    case SettingsValue::caret_motion_time:
    case SettingsValue::indent_guide_colors:
    case SettingsValue::indent_guide_width:
    case SettingsValue::pane_resize_step:
        return std::nullopt;
    }
    std::unreachable();
}

EditorSettings settings_choice_applied(EditorSettings settings, SettingsValue value,
                                       std::size_t position, const std::optional<ThemeChoice> &kept)
{
    if (position >= settings_choice_count(settings, value, kept))
    {
        return settings;
    }
    switch (value)
    {
    case SettingsValue::theme:
        settings.theme = theme_at(position, theme_extra(settings.theme, kept));
        return settings;
    case SettingsValue::ui_language:
        settings.ui_language =
            value_at(listed_display_language_choices, position, settings.ui_language);
        return settings;
    case SettingsValue::finder_preview:
        settings.finder_preview = value_at(finder_preview_order, position, settings.finder_preview);
        return settings;
    case SettingsValue::indent_guide_style:
        settings.indent_guide_style =
            value_at(indent_guide_style_order, position, settings.indent_guide_style);
        return settings;
    case SettingsValue::indent_guide_active:
        settings.indent_guide_active =
            value_at(indent_guide_active_order, position, settings.indent_guide_active);
        return settings;
    case SettingsValue::pane_resize_step_vim:
        settings.pane_resize_step_vim =
            value_at(pane_resize_step_vim_order, position, settings.pane_resize_step_vim);
        return settings;
    case SettingsValue::font_family:
    case SettingsValue::font_size:
    case SettingsValue::caret_motion_time:
    case SettingsValue::indent_guide_colors:
    case SettingsValue::indent_guide_width:
    case SettingsValue::pane_resize_step:
        return settings;
    }
    std::unreachable();
}

std::vector<std::string> settings_choice_texts(const EditorSettings &settings, SettingsValue value,
                                               const std::optional<ThemeChoice> &kept,
                                               DisplayLanguage language)
{
    if (settings_choice_count(settings, value, kept) == 0)
    {
        return {};
    }
    const auto said = [language](UiMessage message) { return ui_text(message, language); };
    switch (value)
    {
    case SettingsValue::theme:
        return theme_texts(settings.theme, kept, language);
    case SettingsValue::ui_language:
        return texts_of(listed_display_language_choices, [language](DisplayLanguageChoice choice)
                        { return display_language_choice_detail(choice, language); });
    case SettingsValue::finder_preview:
        return texts_of(finder_preview_order, [&said](FinderPreview preview)
                        { return said(finder_preview_title(preview)); });
    case SettingsValue::indent_guide_style:
        return texts_of(indent_guide_style_order, [&said](IndentGuideStyle style)
                        { return said(indent_guide_style_title(style)); });
    case SettingsValue::indent_guide_active:
        return texts_of(indent_guide_active_order,
                        [&said](bool active) { return said(indent_guide_active_title(active)); });
    case SettingsValue::pane_resize_step_vim:
        return texts_of(pane_resize_step_vim_order, [&said](bool follows)
                        { return said(pane_resize_step_vim_title(follows)); });
    case SettingsValue::font_family:
    case SettingsValue::font_size:
    case SettingsValue::caret_motion_time:
    case SettingsValue::indent_guide_colors:
    case SettingsValue::indent_guide_width:
    case SettingsValue::pane_resize_step:
        return {};
    }
    std::unreachable();
}
} // namespace nenenib::core
