#include "SettingsCodec.hpp"

#include "BuiltinThemes.hpp"
#include "KeyMap.hpp"
#include "KeyValueFields.hpp"
#include "LanguageTable.hpp"
#include "SettingsFields.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
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

// 鍵の名前と DTO の欄の対（同じ位置）。v4 だけの鍵も読むが、旧版にあれば apply_v4_fields
// が拒否する。
constexpr std::array<std::string_view, 12> field_names{
    "version",     "colorscheme", "font_family", "font_size", "guide",    "wrap",
    "ui_language", "lsp",         "explorer",    "root",      "features", "mapleader"};
constexpr std::array<Field, 12> field_members{
    &SettingsFields::version,     &SettingsFields::colorscheme, &SettingsFields::font_family,
    &SettingsFields::font_size,   &SettingsFields::guide,       &SettingsFields::wrap,
    &SettingsFields::ui_language, &SettingsFields::lsp,         &SettingsFields::explorer,
    &SettingsFields::root,        &SettingsFields::features,    &SettingsFields::mapleader};

// 言語ごとのサーバーの命令の鍵 `lsp.<language>.command`（ADR 0160・ADR 0170 の決定 3）。
constexpr std::string_view lsp_prefix = "lsp.";
constexpr std::string_view lsp_suffix = ".command";
// Vim の `map` の行の鍵の接頭辞（ADR 0176 の決定 1）。行の形の検査は core の parse_map_line。
constexpr std::string_view map_prefix = "map.";

// 表示言語の希望値の全値（ADR 0123 の改訂の決定 1）。値を足したら language_text の switch
// が落ちる。
constexpr std::array<core::DisplayLanguageChoice, 7> language_choices{
    core::DisplayLanguageChoice::system,  core::DisplayLanguageChoice::japanese,
    core::DisplayLanguageChoice::english, core::DisplayLanguageChoice::simplified_chinese,
    core::DisplayLanguageChoice::korean,  core::DisplayLanguageChoice::spanish,
    core::DisplayLanguageChoice::german};

[[nodiscard]] std::string_view language_text(core::DisplayLanguageChoice choice) noexcept
{
    switch (choice)
    {
    case core::DisplayLanguageChoice::system:
        return "system";
    case core::DisplayLanguageChoice::japanese:
        return "ja";
    case core::DisplayLanguageChoice::english:
        return "en";
    case core::DisplayLanguageChoice::simplified_chinese:
        return "zh-hans";
    case core::DisplayLanguageChoice::korean:
        return "ko";
    case core::DisplayLanguageChoice::spanish:
        return "es";
    case core::DisplayLanguageChoice::german:
        return "de";
    }
    std::unreachable();
}

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
        return switch_from(fields.wrap).transform(wrapping_of);
    }
    std::unreachable();
}

[[nodiscard]] std::expected<core::DisplayLanguageChoice, Failure>
language_from(std::string_view text)
{
    const auto found = std::ranges::find(language_choices, text, language_text);
    if (found == language_choices.end())
    {
        return std::unexpected(Failure::malformed);
    }
    return *found;
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

[[nodiscard]] bool has_v4_fields(const SettingsFields &fields) noexcept
{
    return fields.ui_language.has_value() || fields.lsp.has_value() ||
           fields.explorer.has_value() || fields.root.has_value() || fields.features.has_value() ||
           !fields.lsp_commands.empty() || !fields.maps.empty() || fields.mapleader.has_value();
}

// `lsp.<language>.command=<値>` の行。言語は LanguageTable の名前だけ、同じ言語の 2 行目は
// 重複、値は空でないことだけを見る（絶対パスかは使う所が見る）。並びは言語の名前の順。
[[nodiscard]] std::expected<std::vector<std::pair<std::string, std::string>>, Failure>
lsp_commands_from(const SettingsFields &fields)
{
    std::vector<std::pair<std::string, std::string>> commands;
    for (const auto &[name, value] : fields.lsp_commands)
    {
        const auto language = core::language_of_name(name);
        if (!language.has_value() || value.empty())
        {
            return std::unexpected(Failure::malformed);
        }
        const std::string canonical(core::language_name(language.value_or(core::Language{})));
        if (std::ranges::any_of(commands, [&](const auto &row) { return row.first == canonical; }))
        {
            return std::unexpected(Failure::malformed);
        }
        commands.emplace_back(canonical, value);
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
        if (!row.has_value())
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

// v4 だけの欄。旧版はどれも持たず（あれば未知の鍵として拒否）、表示言語は日本語（更新で今の表示を
// 変えない・ADR 0123 の決定 3）、ほかは既定。v4 はどれも必須で厳密に読む。
[[nodiscard]] std::expected<void, Failure> apply_v4_fields(core::EditorSettings &settings,
                                                           const SettingsFields &fields,
                                                           SettingsVersion version)
{
    if (version != SettingsVersion::v4)
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
    auto commands = lsp_commands_from(fields);
    if (!language || !lsp || !explorer || !root || !features || !commands)
    {
        return std::unexpected(Failure::malformed);
    }
    settings.ui_language = language.value();
    settings.lsp = lsp.value();
    settings.explorer = explorer.value();
    settings.root = std::move(root).value();
    settings.features = std::move(features).value();
    settings.lsp_commands = std::move(commands).value();
    return apply_maps(settings, fields);
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
    const auto extended = apply_v4_fields(settings, fields, version);
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

// `lsp.<language>.command=<値>` の行を言語の名前の順に（features の後ろ）。
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
    return std::format("version=4\ncolorscheme={}\nfont_family={}\nfont_size={}\nguide={}\n"
                       "wrap={}\nui_language={}\nlsp={}\nexplorer={}\nroot={}\nfeatures={}\n{}{}",
                       theme, settings.font_family.text(), settings.font_size.points(), guide, wrap,
                       language_text(settings.ui_language), switch_text(settings.lsp),
                       switch_text(settings.explorer), root, features_text(settings.features),
                       lsp_commands_text(settings.lsp_commands), maps_text(settings));
}
} // namespace nenenib::adapters::win32
