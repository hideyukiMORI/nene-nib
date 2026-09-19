#include "SettingsCodec.hpp"

#include "BuiltinThemes.hpp"
#include "SettingsFields.hpp"

#include <format>
#include <optional>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::SettingsFailure;

[[nodiscard]] std::string_view trimmed(std::string_view text)
{
    const auto first = text.find_first_not_of(" \t\r\v\f");
    if (first == std::string_view::npos)
    {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\v\f");
    return text.substr(first, last - first + 1);
}

[[nodiscard]] std::expected<void, Failure> assigned(std::optional<std::string_view> &field,
                                                    std::string_view value)
{
    if (field.has_value())
    {
        return std::unexpected(Failure::malformed);
    }
    field = value;
    return {};
}

[[nodiscard]] std::expected<void, Failure>
assign_field(SettingsFields &fields, std::string_view key, std::string_view value)
{
    if (key == "version")
    {
        return assigned(fields.version, value);
    }
    if (key == "colorscheme")
    {
        return assigned(fields.colorscheme, value);
    }
    if (key == "font_family")
    {
        return assigned(fields.font_family, value);
    }
    if (key == "font_size")
    {
        return assigned(fields.font_size, value);
    }
    return std::unexpected(Failure::malformed);
}

[[nodiscard]] std::expected<void, Failure> read_line(SettingsFields &fields, std::string_view line)
{
    const auto text = trimmed(line);
    if (text.empty())
    {
        return {};
    }
    const auto separator = text.find('=');
    if (separator == std::string_view::npos)
    {
        return std::unexpected(Failure::malformed);
    }
    return assign_field(fields, trimmed(text.substr(0, separator)),
                        trimmed(text.substr(separator + 1)));
}

[[nodiscard]] std::expected<SettingsFields, Failure> read_fields(std::string_view bytes)
{
    if (bytes.starts_with("\xEF\xBB\xBF"))
    {
        bytes.remove_prefix(3);
    }
    SettingsFields fields;
    while (!bytes.empty())
    {
        const auto end = bytes.find('\n');
        const auto assigned_line = read_line(fields, bytes.substr(0, end));
        if (!assigned_line)
        {
            return std::unexpected(assigned_line.error());
        }
        if (end == std::string_view::npos)
        {
            break;
        }
        bytes.remove_prefix(end + 1);
    }
    return fields;
}

[[nodiscard]] std::expected<std::optional<core::BuiltinTheme>, Failure>
theme_from(std::string_view name)
{
    if (name == "system")
    {
        return std::nullopt;
    }
    const auto found = core::theme_named(name);
    if (!found.has_value())
    {
        return std::unexpected(Failure::unknown_theme);
    }
    return found;
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

[[nodiscard]] std::expected<core::EditorSettings, Failure> validated(const SettingsFields &fields)
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
    const auto theme = theme_from(fields.colorscheme.value());
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

std::expected<core::EditorSettings, Failure> decode_settings(std::string_view bytes)
{
    const auto fields = read_fields(bytes);
    if (!fields)
    {
        return std::unexpected(fields.error());
    }
    return validated(fields.value());
}

std::string encode_settings(const core::EditorSettings &settings)
{
    const auto theme = settings.theme.has_value() ? core::theme_of(settings.theme.value()).name
                                                  : std::string_view{"system"};
    return std::format("version=1\ncolorscheme={}\nfont_family={}\nfont_size={}\n", theme,
                       settings.font_family.text(), settings.font_size.points());
}
} // namespace nenenib::adapters::win32
