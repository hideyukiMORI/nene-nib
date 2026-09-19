#include "KeyValueFields.hpp"

namespace nenenib::adapters::win32
{
namespace
{
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

[[nodiscard]] std::expected<void, KeyValueFailure> read_line(KeyValueFields &fields,
                                                             std::string_view line)
{
    const auto text = trimmed(line);
    if (text.empty())
    {
        return {};
    }
    const auto separator = text.find('=');
    if (separator == std::string_view::npos)
    {
        return std::unexpected(KeyValueFailure::malformed);
    }
    const auto key = trimmed(text.substr(0, separator));
    if (key.empty())
    {
        return std::unexpected(KeyValueFailure::malformed);
    }
    const auto [position, inserted] = fields.emplace(key, trimmed(text.substr(separator + 1)));
    static_cast<void>(position);
    if (!inserted)
    {
        return std::unexpected(KeyValueFailure::duplicate);
    }
    return {};
}
} // namespace

std::expected<KeyValueFields, KeyValueFailure> key_value_fields(std::string_view bytes)
{
    if (bytes.starts_with("\xEF\xBB\xBF"))
    {
        bytes.remove_prefix(3);
    }
    KeyValueFields fields;
    while (!bytes.empty())
    {
        const auto end = bytes.find('\n');
        const auto assigned = read_line(fields, bytes.substr(0, end));
        if (!assigned)
        {
            return std::unexpected(assigned.error());
        }
        if (end == std::string_view::npos)
        {
            break;
        }
        bytes.remove_prefix(end + 1);
    }
    return fields;
}
} // namespace nenenib::adapters::win32
