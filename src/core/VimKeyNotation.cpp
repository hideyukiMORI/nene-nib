#include "VimKeyNotation.hpp"

#include "VimKeyName.hpp"
#include "VimKeyNames.hpp"

#include <variant>

namespace nenenib::core
{
namespace
{
[[nodiscard]] VimKey key_of(const VimNamedKey &named)
{
    return std::visit([](auto key) { return VimKey{key}; }, named);
}
} // namespace

std::optional<VimKey> vim_key_of_name(std::string_view name)
{
    for (const VimKeyName &row : vim_key_names)
    {
        if (row.text == name)
        {
            return key_of(row.key);
        }
    }
    return std::nullopt;
}

std::optional<std::string_view> vim_key_name_of(const VimKey &key)
{
    for (const VimKeyName &row : vim_key_names)
    {
        const bool same = std::visit([&key](auto named) { return key == VimKey{named}; }, row.key);
        if (same)
        {
            return row.text;
        }
    }
    return std::nullopt;
}
} // namespace nenenib::core
