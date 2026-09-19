#include "SettingsCodec.hpp"

#include "BuiltinThemes.hpp"
#include "KeyValueFields.hpp"
#include "SettingsFields.hpp"

#include <format>
#include <optional>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::SettingsFailure;

[[nodiscard]] std::expected<void, Failure>
assign_field(SettingsFields &fields, std::string_view key, std::string_view value)
{
    if (key == "version")
    {
        fields.version = value;
        return {};
    }
    if (key == "colorscheme")
    {
        fields.colorscheme = value;
        return {};
    }
    if (key == "font_family")
    {
        fields.font_family = value;
        return {};
    }
    if (key == "font_size")
    {
        fields.font_size = value;
        return {};
    }
    return std::unexpected(Failure::malformed);
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
        const auto assigned = assign_field(fields, key, value);
        if (!assigned)
        {
            return std::unexpected(assigned.error());
        }
    }
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

[[nodiscard]] std::expected<core::EditorSettings, application::SettingsIssue>
validated(const SettingsFields &fields, const core::ThemeCatalog &themes)
{
    if (!fields.version.has_value() || !fields.colorscheme.has_value() ||
        !fields.font_family.has_value() || !fields.font_size.has_value())
    {
        return std::unexpected(Failure::malformed);
    }
    if (fields.version.value() != "1")
    {
        return std::unexpected(Failure::unsupported_version);
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
    return core::EditorSettings{size.value(), family.value(), theme.value()};
}
} // namespace

std::expected<core::EditorSettings, application::SettingsIssue>
decode_settings(std::string_view bytes, const core::ThemeCatalog &themes)
{
    const auto fields = read_fields(bytes);
    if (!fields)
    {
        return std::unexpected(fields.error());
    }
    return validated(fields.value(), themes);
}

std::string encode_settings(const core::EditorSettings &settings)
{
    const auto theme =
        settings.theme.has_value() ? settings.theme.value().name() : std::string_view{"system"};
    return std::format("version=1\ncolorscheme={}\nfont_family={}\nfont_size={}\n", theme,
                       settings.font_family.text(), settings.font_size.points());
}
} // namespace nenenib::adapters::win32
