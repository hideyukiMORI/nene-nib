#include "ThemeCodec.hpp"

#include "BuiltinThemes.hpp"
#include "ColorCodec.hpp"
#include "KeyValueFields.hpp"
#include "ThemeColorFields.hpp"
#include "ThemeContrast.hpp"
#include "ThemeDerivation.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = core::ThemeFailure;
constexpr std::array<std::string_view, 6> metadata_keys{"version", "name",    "appearance",
                                                        "author",  "license", "url"};

[[nodiscard]] bool known_key(std::string_view key)
{
    const auto matches = [key](const auto &field) { return field.key == key; };
    return std::find(metadata_keys.begin(), metadata_keys.end(), key) != metadata_keys.end() ||
           std::any_of(body_color_fields.begin(), body_color_fields.end(), matches) ||
           std::any_of(ui_rgb_fields.begin(), ui_rgb_fields.end(), matches) ||
           std::any_of(ui_rgba_fields.begin(), ui_rgba_fields.end(), matches);
}

[[nodiscard]] Failure field_failure(KeyValueFailure failure)
{
    switch (failure)
    {
    case KeyValueFailure::malformed:
        return Failure::malformed;
    case KeyValueFailure::duplicate:
        return Failure::duplicate_key;
    }
    std::unreachable();
}

[[nodiscard]] std::expected<KeyValueFields, Failure> checked_fields(std::string_view bytes)
{
    const auto fields = key_value_fields(bytes);
    if (!fields)
    {
        return std::unexpected(field_failure(fields.error()));
    }
    for (const auto &[key, value] : fields.value())
    {
        static_cast<void>(value);
        if (!known_key(key))
        {
            return std::unexpected(Failure::unknown_key);
        }
    }
    for (const auto key : metadata_keys)
    {
        if (!fields.value().contains(key))
        {
            return std::unexpected(Failure::missing_field);
        }
    }
    if (fields.value().at("version") != "1")
    {
        return std::unexpected(Failure::unsupported_version);
    }
    return fields.value();
}

[[nodiscard]] std::expected<core::ThemeName, Failure> checked_name(std::string_view text,
                                                                   const core::ThemeName &expected)
{
    const auto name = core::ThemeName::parse(text);
    if (!name)
    {
        return std::unexpected(Failure::invalid_name);
    }
    if (name.value() != expected)
    {
        return std::unexpected(Failure::name_mismatch);
    }
    if (name.value().text() == "system" || core::theme_named(name.value().text()).has_value())
    {
        return std::unexpected(Failure::reserved_name);
    }
    return name.value();
}

[[nodiscard]] std::expected<core::Appearance, Failure> appearance_from(std::string_view text)
{
    if (text == "dark")
    {
        return core::Appearance::dark;
    }
    if (text == "light")
    {
        return core::Appearance::light;
    }
    return std::unexpected(Failure::invalid_appearance);
}

[[nodiscard]] std::expected<core::OwnedThemeSource, Failure>
source_from(const KeyValueFields &fields)
{
    const auto author = core::DisplayText::parse(fields.at("author"));
    const auto license = core::DisplayText::parse(fields.at("license"));
    const auto url = core::DisplayText::parse(fields.at("url"));
    if (!author || !license || !url)
    {
        return std::unexpected(Failure::invalid_text);
    }
    return core::OwnedThemeSource{author.value(), license.value(), url.value()};
}

[[nodiscard]] std::expected<core::SyntaxPalette, Failure> body_from(const KeyValueFields &fields)
{
    core::SyntaxPalette body{};
    for (const auto &field : body_color_fields)
    {
        const auto found = fields.find(field.key);
        if (found == fields.end())
        {
            return std::unexpected(Failure::missing_field);
        }
        const auto color = decode_rgb(found->second);
        if (!color)
        {
            return std::unexpected(color.error());
        }
        body.*(field.member) = color.value();
    }
    return body;
}

template <typename Color, std::size_t N, typename Parse>
[[nodiscard]] std::expected<void, Failure>
override_colors(core::Palette &ui, const KeyValueFields &fields,
                const std::array<ColorField<Color, core::Palette>, N> &table, Parse parse)
{
    for (const auto &field : table)
    {
        const auto found = fields.find(field.key);
        if (found == fields.end())
        {
            continue;
        }
        const auto color = parse(found->second);
        if (!color)
        {
            return std::unexpected(color.error());
        }
        ui.*(field.member) = color.value();
    }
    return {};
}

[[nodiscard]] std::expected<core::Palette, Failure>
ui_from(const KeyValueFields &fields, const core::SyntaxPalette &body, core::Appearance appearance)
{
    auto ui = core::derive_ui(body.background, body.foreground, body.cursor, appearance);
    const auto rgb = override_colors(ui, fields, ui_rgb_fields, decode_rgb);
    if (!rgb)
    {
        return std::unexpected(rgb.error());
    }
    const auto rgba = override_colors(ui, fields, ui_rgba_fields, decode_rgba);
    if (!rgba)
    {
        return std::unexpected(rgba.error());
    }
    return ui;
}

[[nodiscard]] std::expected<core::ThemeDocument, Failure>
document_from(const KeyValueFields &fields, const core::ThemeName &expected)
{
    const auto name = checked_name(fields.at("name"), expected);
    if (!name)
    {
        return std::unexpected(name.error());
    }
    const auto appearance = appearance_from(fields.at("appearance"));
    if (!appearance)
    {
        return std::unexpected(appearance.error());
    }
    const auto source = source_from(fields);
    if (!source)
    {
        return std::unexpected(source.error());
    }
    const auto body = body_from(fields);
    if (!body)
    {
        return std::unexpected(body.error());
    }
    const auto ui = ui_from(fields, body.value(), appearance.value());
    if (!ui)
    {
        return std::unexpected(ui.error());
    }
    const core::ThemeDocument document{name.value(), appearance.value(), ui.value(), body.value(),
                                       source.value()};
    if (!theme_has_contrast(core::theme_view(document)))
    {
        return std::unexpected(Failure::insufficient_contrast);
    }
    return document;
}
} // namespace

std::expected<core::ThemeName, Failure> theme_name_for_file(const core::FilePath &path)
{
    constexpr std::string_view suffix = ".v1.theme";
    const auto filename = path.file_name();
    if (!filename.ends_with(suffix))
    {
        return std::unexpected(Failure::invalid_name);
    }
    const auto stem = filename.substr(0, filename.size() - suffix.size());
    const auto name = core::ThemeName::parse(stem);
    if (!name || name.value().text() != stem)
    {
        return std::unexpected(Failure::invalid_name);
    }
    return name.value();
}

std::expected<core::ThemeDocument, Failure> decode_theme(std::string_view bytes,
                                                         const core::ThemeName &expected_name)
{
    if (bytes.size() > maximum_theme_bytes)
    {
        return std::unexpected(Failure::too_large);
    }
    if (!core::validate_utf8(bytes))
    {
        return std::unexpected(Failure::invalid_text);
    }
    const auto fields = checked_fields(bytes);
    if (!fields)
    {
        return std::unexpected(fields.error());
    }
    return document_from(fields.value(), expected_name);
}

std::expected<core::ThemeDocument, Failure> load_theme(application::FilePort &files,
                                                       const core::FilePath &path)
{
    const auto name = theme_name_for_file(path);
    if (!name)
    {
        return std::unexpected(name.error());
    }
    const auto bytes = files.read(path, maximum_theme_bytes);
    if (!bytes)
    {
        if (bytes.error() == application::FileFailure::not_found)
        {
            return std::unexpected(Failure::not_found);
        }
        if (bytes.error() == application::FileFailure::too_large)
        {
            return std::unexpected(Failure::too_large);
        }
        return std::unexpected(Failure::unreadable);
    }
    return decode_theme(bytes.value(), name.value());
}
} // namespace nenenib::adapters::win32
