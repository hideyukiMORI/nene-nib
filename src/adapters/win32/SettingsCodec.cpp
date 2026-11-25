#include "SettingsCodec.hpp"

#include "BuiltinThemes.hpp"
#include "CaretGlide.hpp"
#include "DisplayLanguageNames.hpp"
#include "ExplorerLayout.hpp"
#include "FinderPreview.hpp"
#include "IndentGuide.hpp"
#include "Invisibles.hpp"
#include "KeyMap.hpp"
#include "KeyValueFields.hpp"
#include "Language.hpp"
#include "LanguageTable.hpp"
#include "PaneResizeStep.hpp"
#include "SettingsFields.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::SettingsFailure;
using Field = std::optional<std::string_view> SettingsFields::*;

// 鍵の名前と DTO の欄の対（同じ位置）。v4・v5・v6・v7 だけの鍵も読むが、旧版にあれば
// apply_v4_fields・apply_v5_fields・apply_v6_fields・apply_v7_fields が拒否する。
constexpr std::array<std::string_view, 32> field_names{"version",
                                                       "colorscheme",
                                                       "font_family",
                                                       "font_size",
                                                       "guide",
                                                       "wrap",
                                                       "ui_language",
                                                       "lsp",
                                                       "explorer",
                                                       "root",
                                                       "features",
                                                       "mapleader",
                                                       "ignorecase",
                                                       "smartcase",
                                                       "relativenumber",
                                                       "scrolloff",
                                                       "explorer_width",
                                                       "autoindent",
                                                       "finder_preview",
                                                       "caret_motion",
                                                       "caret_motion_time",
                                                       "caret_sparks",
                                                       "pane_resize_step",
                                                       "pane_resize_step_vim",
                                                       "indent_guide_style",
                                                       "indent_guide_color_count",
                                                       "indent_guide_highlight_current",
                                                       "indent_guide_width",
                                                       "list_eol",
                                                       "list_tab",
                                                       "list_wide_space",
                                                       "list_space"};
constexpr std::array<Field, 32> field_members{&SettingsFields::version,
                                              &SettingsFields::colorscheme,
                                              &SettingsFields::font_family,
                                              &SettingsFields::font_size,
                                              &SettingsFields::guide,
                                              &SettingsFields::wrap,
                                              &SettingsFields::ui_language,
                                              &SettingsFields::lsp,
                                              &SettingsFields::explorer,
                                              &SettingsFields::root,
                                              &SettingsFields::features,
                                              &SettingsFields::mapleader,
                                              &SettingsFields::ignorecase,
                                              &SettingsFields::smartcase,
                                              &SettingsFields::relativenumber,
                                              &SettingsFields::scrolloff,
                                              &SettingsFields::explorer_width,
                                              &SettingsFields::autoindent,
                                              &SettingsFields::finder_preview,
                                              &SettingsFields::caret_motion,
                                              &SettingsFields::caret_motion_time,
                                              &SettingsFields::caret_sparks,
                                              &SettingsFields::pane_resize_step,
                                              &SettingsFields::pane_resize_step_vim,
                                              &SettingsFields::indent_guide_style,
                                              &SettingsFields::indent_guide_color_count,
                                              &SettingsFields::indent_guide_highlight_current,
                                              &SettingsFields::indent_guide_width,
                                              &SettingsFields::list_eol,
                                              &SettingsFields::list_tab,
                                              &SettingsFields::list_wide_space,
                                              &SettingsFields::list_space};

// 言語ごとのサーバーの命令の鍵 `lsp.<language>.command`（ADR 0160・ADR 0170 の決定 3）。
constexpr std::string_view lsp_prefix = "lsp.";
constexpr std::string_view lsp_suffix = ".command";
// Vim の `map` の行の鍵の接頭辞（ADR 0176 の決定 1）。行の形の検査は core の parse_map_line。
constexpr std::string_view map_prefix = "map.";

[[nodiscard]] std::expected<SettingsFields, Failure> read_fields(std::string_view bytes)
{
    const auto parsed = key_value_fields(bytes);
    if (!parsed)
    {
        return std::unexpected(Failure::malformed);
    }
    SettingsFields fields;
    for (const auto &[key, value] : parsed.value())
    {
        if (key.starts_with(lsp_prefix) && key.ends_with(lsp_suffix) &&
            key.size() > lsp_prefix.size() + lsp_suffix.size())
        {
            fields.lsp_commands.emplace_back(
                key.substr(lsp_prefix.size(), key.size() - lsp_prefix.size() - lsp_suffix.size()),
                value);
            continue;
        }
        if (key.starts_with(map_prefix))
        {
            fields.maps.emplace_back(key, value);
            continue;
        }
        const auto found = std::ranges::find(field_names, key);
        if (found == field_names.end())
        {
            return std::unexpected(Failure::malformed);
        }
        const auto index = static_cast<std::size_t>(found - field_names.begin());
        fields.*field_members.at(index) = value;
    }
    // 鍵の表は名前の順なので、`map` の行はファイルの中の位置（借りた bytes の中の鍵の先頭）の
    // 順に戻す。同じモードと左辺は後の行が勝つ（ADR 0176 の決定 1）。
    std::ranges::sort(fields.maps, std::less<>{}, [](const auto &row) { return row.first.data(); });
    return fields;
}

[[nodiscard]] std::expected<std::optional<core::ThemeChoice>, application::SettingsIssue>
theme_from(std::string_view name, const core::ThemeCatalog &themes)
{
    if (name == "system")
    {
        return std::nullopt;
    }
    const auto parsed = core::ThemeName::parse(name);
    if (!parsed)
    {
        return std::unexpected(Failure::unknown_theme);
    }
    const auto found = themes.find(parsed.value());
    if (!found)
    {
        return std::unexpected(found.error());
    }
    return found.value();
}

[[nodiscard]] std::expected<core::FontSize, Failure> size_from(std::string_view text)
{
    const auto size = core::FontSize::parse(text);
    if (!size)
    {
        return std::unexpected(Failure::invalid_font_size);
    }
    return size.value();
}

// `on` か `off` だけを受ける（大文字・空・ほかの語は拒否）。
[[nodiscard]] std::expected<bool, Failure> switch_from(const std::optional<std::string_view> &value)
{
    if (value == "on")
    {
        return true;
    }
    if (value == "off")
    {
        return false;
    }
    return std::unexpected(Failure::malformed);
}

[[nodiscard]] core::GuideVisibility guide_of(bool shown) noexcept
{
    return shown ? core::GuideVisibility::shown : core::GuideVisibility::hidden;
}

[[nodiscard]] core::LineWrapping wrapping_of(bool wrap) noexcept
{
    return wrap ? core::LineWrapping::wrap : core::LineWrapping::no_wrap;
}

[[nodiscard]] std::expected<core::GuideVisibility, Failure> guide_from(const SettingsFields &fields,
                                                                       SettingsVersion version)
{
    switch (version)
    {
    case SettingsVersion::v1:
        if (fields.guide.has_value())
        {
            return std::unexpected(Failure::malformed);
        }
        return core::GuideVisibility::shown;
    case SettingsVersion::v2:
    case SettingsVersion::v3:
    case SettingsVersion::v4:
    case SettingsVersion::v5:
    case SettingsVersion::v6:
    case SettingsVersion::v7:
        return switch_from(fields.guide).transform(guide_of);
    }
    std::unreachable();
}

[[nodiscard]] std::expected<core::LineWrapping, Failure> wrap_from(const SettingsFields &fields,
                                                                   SettingsVersion version)
{
    switch (version)
    {
    case SettingsVersion::v1:
    case SettingsVersion::v2:
        if (fields.wrap.has_value())
        {
            return std::unexpected(Failure::malformed);
        }
        return core::LineWrapping::wrap;
    case SettingsVersion::v3:
    case SettingsVersion::v4:
    case SettingsVersion::v5:
    case SettingsVersion::v6:
    case SettingsVersion::v7:
        return switch_from(fields.wrap).transform(wrapping_of);
    }
    std::unreachable();
}

[[nodiscard]] std::expected<core::DisplayLanguageChoice, Failure>
language_from(std::string_view text)
{
    // 名前の正本は core の 1 か所（`:set language=` と同じ・#1019）。
    const auto found = core::display_language_choice_named(text);
    if (!found.has_value())
    {
        return std::unexpected(Failure::malformed);
    }
    return found.value();
}

// `root=` は「決めていない」、ほかは検証済みの経路（展開も絶対化もしない・ARC-007）。
[[nodiscard]] std::expected<std::optional<core::FilePath>, Failure> root_from(std::string_view text)
{
    if (text.empty())
    {
        return std::nullopt;
    }
    auto path = core::FilePath::parse(text);
    if (!path)
    {
        return std::unexpected(Failure::malformed);
    }
    return std::optional<core::FilePath>{std::move(path).value()};
}

[[nodiscard]] bool feature_character(char letter) noexcept
{
    return (letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9') || letter == '_';
}

// `features=a,b,c`。名前は ASCII の小文字・数字・`_` で、空の名前と重複は拒否する。中身の意味は
// 解釈しない（ADR 0170 の決定 3・機能の表は後の PR）。
[[nodiscard]] std::expected<std::vector<std::string>, Failure> features_from(std::string_view text)
{
    std::vector<std::string> names;
    if (text.empty())
    {
        return names;
    }
    while (true)
    {
        const auto comma = text.find(',');
        const auto name = text.substr(0, comma);
        if (name.empty() || !std::ranges::all_of(name, feature_character) ||
            std::ranges::find(names, name) != names.end())
        {
            return std::unexpected(Failure::malformed);
        }
        names.emplace_back(name);
        if (comma == std::string_view::npos)
        {
            return names;
        }
        text.remove_prefix(comma + 1);
    }
}

// v4 の最初の定義（#1055）の後に足した on/off の鍵。無ければ既定の off、有れば厳密に読む
// （ADR 0170 決定 3 の追補・#1083）。書くときは必ず書く。
[[nodiscard]] std::expected<bool, Failure>
later_switch_from(const std::optional<std::string_view> &value)
{
    if (!value.has_value())
    {
        return false;
    }
    return switch_from(value);
}

// 追従の余白も後から足した鍵（#1041）。無ければ 0、有れば core の読み 1 本で厳密に読む。
[[nodiscard]] std::expected<std::size_t, Failure>
later_scrolloff_from(const std::optional<std::string_view> &value)
{
    if (!value.has_value())
    {
        return 0;
    }
    const auto parsed = core::scrolloff_of(value.value());
    if (!parsed.has_value())
    {
        return std::unexpected(Failure::malformed);
    }
    return parsed.value();
}

// 面の幅も後から足した鍵（ADR 0162 の追補・施主決定 D78）。無い・空・形の違う値はどれも既定の式
// （値なし）で、設定の全体を拒まない（D78 の「壊れた値は既定」・ADR 0170 の追補）。値の読みは core
// の explorer_width_of の 1 本。
[[nodiscard]] std::optional<std::int32_t>
later_explorer_width_from(const std::optional<std::string_view> &value) noexcept
{
    return value.has_value() ? core::explorer_width_of(value.value()) : std::nullopt;
}

[[nodiscard]] bool has_v4_fields(const SettingsFields &fields) noexcept
{
    return fields.ui_language.has_value() || fields.lsp.has_value() ||
           fields.explorer.has_value() || fields.root.has_value() || fields.features.has_value() ||
           fields.ignorecase.has_value() || fields.smartcase.has_value() ||
           fields.relativenumber.has_value() || fields.scrolloff.has_value() ||
           fields.explorer_width.has_value() || !fields.lsp_commands.empty() ||
           !fields.maps.empty() || fields.mapleader.has_value();
}

// 名前の形（ASCII の小文字・数字・`+` `-` `_` の 1〜32 字・ADR 0170 の 2026-11-20 の追補）。
[[nodiscard]] bool command_name_shaped(std::string_view name) noexcept
{
    constexpr std::size_t longest_command_name = 32;
    return !name.empty() && name.size() <= longest_command_name &&
           std::ranges::all_of(name,
                               [](char character)
                               {
                                   return (character >= 'a' && character <= 'z') ||
                                          (character >= '0' && character <= '9') ||
                                          character == '+' || character == '-' || character == '_';
                               });
}

// v5 の鍵の集合を持つ版か（v6 は v5 の全部の鍵を同じ形で持ち、v7 は v6 の全部の鍵を同じ形で
// 持つ・ADR 0192 の決定 1・ADR 0193 の 2026-11-24 の追補 2）。
[[nodiscard]] bool carries_v5_fields(SettingsVersion version) noexcept
{
    switch (version)
    {
    case SettingsVersion::v1:
    case SettingsVersion::v2:
    case SettingsVersion::v3:
    case SettingsVersion::v4:
        return false;
    case SettingsVersion::v5:
    case SettingsVersion::v6:
    case SettingsVersion::v7:
        return true;
    }
    std::unreachable();
}

// 1.0.0 が知る言語の名前か（Language の先頭から dockerfile までの 26 値・D91）。後の版が足した
// 言語（Java など・ADR 0154 の 2026-11-20 の追補）の名前は v4 では受けない（v4 の読みを緩めない）。
[[nodiscard]] bool released_language_name(std::string_view name) noexcept
{
    const auto language = core::language_of_name(name);
    return language.has_value() &&
           std::to_underlying(language.value()) <= std::to_underlying(core::Language::dockerfile);
}

// `lsp.<language>.command=<値>` の行。同じ名前の 2 行目は重複、値は空でないことだけを見る（絶対
// パスかは使う所が見る）。v4 は言語を 1.0.0 の LanguageTable の名前だけ受ける（同じ厳しさ・D91）。
// v5 と v6 は名前の形だけを厳密に読み、表に無い名前の行も持って書き戻す（後の版が足した言語の行を
// 1.1.0 が読めて消さない・使うのは表の名前の行だけ・ADR 0170 の 2026-11-20
// の追補）。並びは名前の順。
[[nodiscard]] std::expected<std::vector<std::pair<std::string, std::string>>, Failure>
lsp_commands_from(const SettingsFields &fields, SettingsVersion version)
{
    std::vector<std::pair<std::string, std::string>> commands;
    for (const auto &[name, value] : fields.lsp_commands)
    {
        const bool named = carries_v5_fields(version) ? core::language_of_name(name).has_value() ||
                                                            command_name_shaped(name)
                                                      : released_language_name(name);
        if (!named || value.empty() ||
            std::ranges::any_of(commands, [name](const auto &row) { return row.first == name; }))
        {
            return std::unexpected(Failure::malformed);
        }
        commands.emplace_back(name, value);
    }
    std::ranges::sort(commands, {},
                      [](const auto &row) -> const std::string & { return row.first; });
    return commands;
}

// `map.<modes>.<lhs>=<rhs>` の行と `mapleader=`（ADR 0176 の決定 1）。0 行以上・壊れた行は
// 設定の全体を拒む（ADR 0114）。`mapleader` が無ければ既定の `\`。
[[nodiscard]] std::expected<void, Failure> apply_maps(core::EditorSettings &settings,
                                                      const SettingsFields &fields)
{
    std::vector<core::MapRow> rows;
    rows.reserve(fields.maps.size());
    for (const auto &[key, value] : fields.maps)
    {
        auto row = core::parse_map_line(key, value);
        // 書き戻す鍵はモードの文字を並べ直した正準の鍵なので、同じ鍵の 2 行はそれで判定する
        // （`map.vn.x` と `map.nv.x` を通すと、保存で同じ鍵の 2 行を書いてしまう）。
        if (!row.has_value() ||
            std::ranges::any_of(
                rows, [&row](const core::MapRow &earlier)
                { return core::map_line_key(earlier) == core::map_line_key(row.value()); }))
        {
            return std::unexpected(Failure::malformed);
        }
        rows.push_back(std::move(row).value());
    }
    if (fields.mapleader.has_value())
    {
        if (!core::parse_map_leader(fields.mapleader.value()).has_value())
        {
            return std::unexpected(Failure::malformed);
        }
        settings.mapleader = std::string(fields.mapleader.value());
    }
    settings.maps = std::move(rows);
    return {};
}

// v4 の鍵の集合を持つ版か（v5 は v4 の全部の鍵を同じ形で持つ・ADR 0170 の 2026-11-16 の追補）。
[[nodiscard]] bool carries_v4_fields(SettingsVersion version) noexcept
{
    switch (version)
    {
    case SettingsVersion::v1:
    case SettingsVersion::v2:
    case SettingsVersion::v3:
        return false;
    case SettingsVersion::v4:
    case SettingsVersion::v5:
    case SettingsVersion::v6:
    case SettingsVersion::v7:
        return true;
    }
    std::unreachable();
}

// v4 からの欄。旧版はどれも持たず（あれば未知の鍵として拒否）、表示言語は日本語（更新で今の表示を
// 変えない・ADR 0123 の決定 3）、ほかは既定。v4 と v5 はどれも必須で厳密に読む。
[[nodiscard]] std::expected<void, Failure> apply_v4_fields(core::EditorSettings &settings,
                                                           const SettingsFields &fields,
                                                           SettingsVersion version)
{
    if (!carries_v4_fields(version))
    {
        if (has_v4_fields(fields))
        {
            return std::unexpected(Failure::malformed);
        }
        settings.ui_language = core::DisplayLanguageChoice::japanese;
        return {};
    }
    if (!fields.ui_language.has_value() || !fields.root.has_value() || !fields.features.has_value())
    {
        return std::unexpected(Failure::malformed);
    }
    const auto language = language_from(fields.ui_language.value());
    const auto lsp = switch_from(fields.lsp);
    const auto explorer = switch_from(fields.explorer);
    auto root = root_from(fields.root.value());
    auto features = features_from(fields.features.value());
    auto commands = lsp_commands_from(fields, version);
    const auto ignorecase = later_switch_from(fields.ignorecase);
    const auto smartcase = later_switch_from(fields.smartcase);
    const auto relativenumber = later_switch_from(fields.relativenumber);
    const auto scrolloff = later_scrolloff_from(fields.scrolloff);
    if (!language || !lsp || !explorer || !root || !features || !commands || !ignorecase ||
        !smartcase || !relativenumber || !scrolloff)
    {
        return std::unexpected(Failure::malformed);
    }
    settings.ui_language = language.value();
    settings.lsp = lsp.value();
    settings.explorer = explorer.value();
    settings.root = std::move(root).value();
    settings.features = std::move(features).value();
    settings.lsp_commands = std::move(commands).value();
    settings.ignorecase = ignorecase.value();
    settings.smartcase = smartcase.value();
    settings.relativenumber = relativenumber.value();
    settings.scrolloff = scrolloff.value();
    settings.explorer_width = later_explorer_width_from(fields.explorer_width);
    return apply_maps(settings, fields);
}

// v5 だけの鍵（ADR 0170 の 2026-11-16 の追補・施主決定 D91）。v4 以前に在れば 1.0.0 と同じく
// 未知の鍵として拒否し（v4 の読みは 1.0.0 と同じ厳しさ）、無ければ既定を補う。v5 は後から足した
// 鍵の形（無ければ既定・有れば厳密）で読む。
[[nodiscard]] std::expected<void, Failure> apply_v5_fields(core::EditorSettings &settings,
                                                           const SettingsFields &fields,
                                                           SettingsVersion version)
{
    if (!carries_v5_fields(version))
    {
        if (fields.autoindent.has_value() || fields.finder_preview.has_value())
        {
            return std::unexpected(Failure::malformed);
        }
        return {};
    }
    const auto autoindent = later_switch_from(fields.autoindent);
    if (!autoindent)
    {
        return std::unexpected(autoindent.error());
    }
    settings.autoindent = autoindent.value();
    // プレビュー欄の置き方（ADR 0189 の決定 1）。無ければ既定の auto、有れば名前の表の 4 つだけ。
    if (fields.finder_preview.has_value())
    {
        const auto preview = core::finder_preview_named(fields.finder_preview.value());
        if (!preview.has_value())
        {
            return std::unexpected(Failure::malformed);
        }
        settings.finder_preview = preview.value();
    }
    return {};
}

[[nodiscard]] bool has_v6_fields(const SettingsFields &fields) noexcept
{
    return fields.caret_motion.has_value() || fields.caret_motion_time.has_value() ||
           fields.caret_sparks.has_value() || fields.pane_resize_step.has_value() ||
           fields.pane_resize_step_vim.has_value() || fields.indent_guide_style.has_value() ||
           fields.indent_guide_color_count.has_value() ||
           fields.indent_guide_highlight_current.has_value() ||
           fields.indent_guide_width.has_value();
}

// 後から足した値の鍵（無ければ既定のまま・有れば core の読み 1 本で厳密に・ADR 0170 の追補）。
template <typename Value, typename Reader>
[[nodiscard]] std::expected<void, Failure>
later_value_from(const std::optional<std::string_view> &text, Value &value, Reader reader)
{
    if (!text.has_value())
    {
        return {};
    }
    const auto parsed = reader(text.value());
    if (!parsed.has_value())
    {
        return std::unexpected(Failure::malformed);
    }
    value = parsed.value();
    return {};
}

// インデントガイドの形の v6 の鍵（ADR 0193 の決定 4）。無い鍵は既定（rainbow・4 色・強める・4
// 桁）。
[[nodiscard]] std::expected<void, Failure> apply_indent_guide_fields(core::EditorSettings &settings,
                                                                     const SettingsFields &fields)
{
    const auto active = fields.indent_guide_highlight_current.has_value()
                            ? switch_from(fields.indent_guide_highlight_current)
                            : std::expected<bool, Failure>{settings.indent_guide_highlight_current};
    if (!active)
    {
        return std::unexpected(active.error());
    }
    settings.indent_guide_highlight_current = active.value();
    const auto style = later_value_from(fields.indent_guide_style, settings.indent_guide_style,
                                        core::indent_guide_style_named);
    const auto colors =
        later_value_from(fields.indent_guide_color_count, settings.indent_guide_color_count,
                         core::indent_guide_color_count_of);
    const auto width = later_value_from(fields.indent_guide_width, settings.indent_guide_width,
                                        core::indent_guide_width_of);
    if (!style || !colors || !width)
    {
        return std::unexpected(Failure::malformed);
    }
    return {};
}

// v6 の枠の大きさの 2 つ（ADR 0148 の 2026-11-23
// の追補・#1972）。後から足した鍵の形（無ければ既定の 5 升と
// off・有れば厳密）で読む。升の数の読みは core の pane_resize_step_of の 1 本。
[[nodiscard]] std::expected<void, Failure> apply_pane_resize_fields(core::EditorSettings &settings,
                                                                    const SettingsFields &fields)
{
    if (fields.pane_resize_step.has_value())
    {
        const auto step = core::pane_resize_step_of(fields.pane_resize_step.value());
        if (!step.has_value())
        {
            return std::unexpected(Failure::malformed);
        }
        settings.pane_resize_step = step.value();
    }
    const auto follows = later_switch_from(fields.pane_resize_step_vim);
    if (!follows)
    {
        return std::unexpected(follows.error());
    }
    settings.pane_resize_step_vim = follows.value();
    return {};
}

// v6 だけの鍵（ADR 0192 の決定 1・ADR 0193 の決定 4）。v5 以前に在れば未知の鍵として拒否し（v5 の
// 読みは 1.1.0 と同じ厳しさ）、無ければ既定（オフ・120 ms・インデントガイドの既定）のまま。v6 は
// 後から足した鍵の形（無ければ既定・有れば厳密）で読む。時間の読みは core の caret_motion_time_of
// の 1 本。打ったときの粒の caret_sparks（ADR 0192 の追補・#1979・既定オフ）と枠の大きさの 2
// つ（#1972）も同じ形。
[[nodiscard]] std::expected<void, Failure> apply_v6_fields(core::EditorSettings &settings,
                                                           const SettingsFields &fields,
                                                           SettingsVersion version)
{
    switch (version)
    {
    case SettingsVersion::v1:
    case SettingsVersion::v2:
    case SettingsVersion::v3:
    case SettingsVersion::v4:
    case SettingsVersion::v5:
        if (has_v6_fields(fields))
        {
            return std::unexpected(Failure::malformed);
        }
        return {};
    case SettingsVersion::v6:
    case SettingsVersion::v7:
        break;
    }
    const auto motion = later_switch_from(fields.caret_motion);
    if (!motion)
    {
        return std::unexpected(motion.error());
    }
    settings.caret_motion = motion.value();
    if (fields.caret_motion_time.has_value())
    {
        const auto time = core::caret_motion_time_of(fields.caret_motion_time.value());
        if (!time.has_value())
        {
            return std::unexpected(Failure::malformed);
        }
        settings.caret_motion_time = time.value();
    }
    const auto sparks = later_switch_from(fields.caret_sparks);
    if (!sparks)
    {
        return std::unexpected(sparks.error());
    }
    settings.caret_sparks = sparks.value();
    const auto panes = apply_pane_resize_fields(settings, fields);
    if (!panes)
    {
        return panes;
    }
    return apply_indent_guide_fields(settings, fields);
}

[[nodiscard]] bool has_v7_fields(const SettingsFields &fields) noexcept
{
    return fields.list_eol.has_value() || fields.list_tab.has_value() ||
           fields.list_wide_space.has_value() || fields.list_space.has_value();
}

// 後から足した on/off の鍵で、無いときの既定がオン（不可視文字の種類のスイッチ）。
[[nodiscard]] std::expected<void, Failure>
later_on_from(const std::optional<std::string_view> &text, bool &value)
{
    if (!text.has_value())
    {
        return {};
    }
    const auto parsed = switch_from(text);
    if (!parsed)
    {
        return std::unexpected(parsed.error());
    }
    value = parsed.value();
    return {};
}

// v7 だけの鍵（ADR 0193 の決定 4 と 2026-11-24 の追補 2・#1977）。v6 以前に在れば未知の鍵として
// 拒否し（v6 の読みは 1.2.0 と同じ厳しさ）、無ければ既定（改行・Tab・全角空白は on・空白は trail）
// のまま。v7 は後から足した鍵の形（無ければ既定・有れば厳密）で読む。空白の名前の読みは core の
// list_space_named の 1 本（`:set listspace=` と共用）。
[[nodiscard]] std::expected<void, Failure> apply_v7_fields(core::EditorSettings &settings,
                                                           const SettingsFields &fields,
                                                           SettingsVersion version)
{
    switch (version)
    {
    case SettingsVersion::v1:
    case SettingsVersion::v2:
    case SettingsVersion::v3:
    case SettingsVersion::v4:
    case SettingsVersion::v5:
    case SettingsVersion::v6:
        if (has_v7_fields(fields))
        {
            return std::unexpected(Failure::malformed);
        }
        return {};
    case SettingsVersion::v7:
        break;
    }
    const auto eol = later_on_from(fields.list_eol, settings.list_eol);
    const auto tab = later_on_from(fields.list_tab, settings.list_tab);
    const auto wide = later_on_from(fields.list_wide_space, settings.list_wide_space);
    const auto space =
        later_value_from(fields.list_space, settings.list_space, core::list_space_named);
    if (!eol || !tab || !wide || !space)
    {
        return std::unexpected(Failure::malformed);
    }
    return {};
}

// v4・v5・v6・v7 の欄を順に置く（どれかが拒めばそこで止める）。
[[nodiscard]] std::expected<void, Failure> apply_later_fields(core::EditorSettings &settings,
                                                              const SettingsFields &fields,
                                                              SettingsVersion version)
{
    const auto extended = apply_v4_fields(settings, fields, version);
    if (!extended)
    {
        return extended;
    }
    const auto later = apply_v5_fields(settings, fields, version);
    if (!later)
    {
        return later;
    }
    const auto sixth = apply_v6_fields(settings, fields, version);
    if (!sixth)
    {
        return sixth;
    }
    return apply_v7_fields(settings, fields, version);
}

[[nodiscard]] std::string_view version_text(SettingsVersion version)
{
    switch (version)
    {
    case SettingsVersion::v1:
        return "1";
    case SettingsVersion::v2:
        return "2";
    case SettingsVersion::v3:
        return "3";
    case SettingsVersion::v4:
        return "4";
    case SettingsVersion::v5:
        return "5";
    case SettingsVersion::v6:
        return "6";
    case SettingsVersion::v7:
        return "7";
    }
    std::unreachable();
}

[[nodiscard]] std::expected<core::EditorSettings, application::SettingsIssue>
validated(const SettingsFields &fields, SettingsVersion version, const core::ThemeCatalog &themes)
{
    if (!fields.version.has_value() || !fields.colorscheme.has_value() ||
        !fields.font_family.has_value() || !fields.font_size.has_value())
    {
        return std::unexpected(Failure::malformed);
    }
    if (fields.version.value() != version_text(version))
    {
        return std::unexpected(Failure::unsupported_version);
    }
    const auto wrap = wrap_from(fields, version);
    if (!wrap)
    {
        return std::unexpected(wrap.error());
    }
    const auto guide = guide_from(fields, version);
    if (!guide)
    {
        return std::unexpected(guide.error());
    }
    const auto theme = theme_from(fields.colorscheme.value(), themes);
    if (!theme)
    {
        return std::unexpected(theme.error());
    }
    const auto family = core::DisplayText::parse(fields.font_family.value());
    if (!family)
    {
        return std::unexpected(Failure::invalid_font_family);
    }
    const auto size = size_from(fields.font_size.value());
    if (!size)
    {
        return std::unexpected(size.error());
    }
    core::EditorSettings settings{size.value(), family.value(), theme.value(), guide.value(),
                                  wrap.value()};
    const auto extended = apply_later_fields(settings, fields, version);
    if (!extended)
    {
        return std::unexpected(extended.error());
    }
    return settings;
}

[[nodiscard]] std::string_view switch_text(bool on) noexcept
{
    return on ? "on" : "off";
}

[[nodiscard]] std::string features_text(const std::vector<std::string> &features)
{
    std::string text;
    for (const auto &name : features)
    {
        if (!text.empty())
        {
            text += ',';
        }
        text += name;
    }
    return text;
}

// `mapleader=` は既定と違うときだけ、`map` の行は持っている順に（lsp の行の後ろ・ADR 0176）。
[[nodiscard]] std::string maps_text(const core::EditorSettings &settings)
{
    std::string text;
    if (settings.mapleader != core::default_editor_settings().mapleader)
    {
        text += std::format("mapleader={}\n", settings.mapleader);
    }
    for (const auto &row : settings.maps)
    {
        text += std::format("{}={}\n", core::map_line_key(row), core::map_line_value(row));
    }
    return text;
}

// `lsp.<language>.command=<値>` の行を言語の名前の順に（固定の鍵の後ろ）。
[[nodiscard]] std::string
lsp_commands_text(const std::vector<std::pair<std::string, std::string>> &commands)
{
    auto sorted = commands;
    std::ranges::sort(sorted, {}, [](const auto &row) -> const std::string & { return row.first; });
    std::string text;
    for (const auto &[language, command] : sorted)
    {
        text += std::format("{}{}{}={}\n", lsp_prefix, language, lsp_suffix, command);
    }
    return text;
}
} // namespace

std::expected<core::EditorSettings, application::SettingsIssue>
decode_settings(std::string_view bytes, SettingsVersion version, const core::ThemeCatalog &themes)
{
    const auto fields = read_fields(bytes);
    if (!fields)
    {
        return std::unexpected(fields.error());
    }
    return validated(fields.value(), version, themes);
}

std::string encode_settings(const core::EditorSettings &settings)
{
    const auto theme =
        settings.theme.has_value() ? settings.theme.value().name() : std::string_view{"system"};
    std::string_view guide;
    switch (settings.guide)
    {
    case core::GuideVisibility::shown:
        guide = "on";
        break;
    case core::GuideVisibility::hidden:
        guide = "off";
        break;
    }
    std::string_view wrap;
    switch (settings.wrapping)
    {
    case core::LineWrapping::wrap:
        wrap = "on";
        break;
    case core::LineWrapping::no_wrap:
        wrap = "off";
        break;
    }
    const auto root = settings.root.has_value() ? settings.root.value().text() : std::string_view{};
    // 面の幅は無ければ空（既定の式）。書くときは必ず書く（ADR 0170 の追補・#1380）。
    const auto explorer_width = settings.explorer_width.has_value()
                                    ? std::to_string(settings.explorer_width.value())
                                    : std::string{};
    // 書くのは v7 だけ（ADR 0193 の 2026-11-24 の追補 2）。v4 の鍵は同じ名前・同じ順で、v5 の鍵は
    // `explorer_width` の後ろ、v6 の鍵は `finder_preview` の後ろ（`caret_motion_time` の後ろに粒の
    // `caret_sparks`・枠の大きさの 2 つ（#1972）・インデントガイドの 4 つの順・ADR 0192 の追補・
    // ADR 0193 の決定 4）、v7 の不可視文字の 4 つは `indent_guide_width` の後ろ・
    // `lsp.<language>.command` の前。
    return std::format(
        "version=7\ncolorscheme={}\nfont_family={}\nfont_size={}\nguide={}\n"
        "wrap={}\nui_language={}\nlsp={}\nexplorer={}\nroot={}\nfeatures={}\n"
        "ignorecase={}\nsmartcase={}\nrelativenumber={}\nscrolloff={}\n"
        "explorer_width={}\nautoindent={}\nfinder_preview={}\ncaret_motion={}\n"
        "caret_motion_time={}\ncaret_sparks={}\npane_resize_step={}\npane_resize_step_vim={}\n"
        "indent_guide_style={}\nindent_guide_color_count={}\nindent_guide_highlight_current={}\n"
        "indent_guide_width={}\nlist_eol={}\nlist_tab={}\nlist_wide_space={}\nlist_space={}\n"
        "{}{}",
        theme, settings.font_family.text(), settings.font_size.points(), guide, wrap,
        core::display_language_choice_name(settings.ui_language), switch_text(settings.lsp),
        switch_text(settings.explorer), root, features_text(settings.features),
        switch_text(settings.ignorecase), switch_text(settings.smartcase),
        switch_text(settings.relativenumber), settings.scrolloff, explorer_width,
        switch_text(settings.autoindent), core::finder_preview_name(settings.finder_preview),
        switch_text(settings.caret_motion), settings.caret_motion_time,
        switch_text(settings.caret_sparks), static_cast<unsigned>(settings.pane_resize_step),
        switch_text(settings.pane_resize_step_vim),
        core::indent_guide_style_name(settings.indent_guide_style),
        settings.indent_guide_color_count, switch_text(settings.indent_guide_highlight_current),
        settings.indent_guide_width, switch_text(settings.list_eol), switch_text(settings.list_tab),
        switch_text(settings.list_wide_space), core::list_space_name(settings.list_space),
        lsp_commands_text(settings.lsp_commands), maps_text(settings));
}
} // namespace nenenib::adapters::win32
