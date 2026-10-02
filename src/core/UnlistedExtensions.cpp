#include "UnlistedExtensions.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
namespace
{
[[nodiscard]] char lower_ascii(char letter) noexcept
{
    return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter + ('a' - 'A')) : letter;
}

// 名前の最後の '.' の後ろ。'.' が無ければ無し。'.' は ASCII なので UTF-8 の継続バイトに現れない。
// STL の後ろからの検索を使わないのは、core の外へシンボルを出さないためである（ARC-003）。
[[nodiscard]] std::optional<std::string_view> extension_of(std::string_view name) noexcept
{
    for (std::size_t index = name.size(); index > 0; --index)
    {
        if (name[index - 1] == '.')
        {
            return name.substr(index);
        }
    }
    return std::nullopt;
}

// 拡張子 extension が表の 1 行 listed（小文字）と、ASCII の大文字と小文字を区別せずに同じか。
[[nodiscard]] bool same_extension(std::string_view extension, std::string_view listed) noexcept
{
    if (extension.size() != listed.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < extension.size(); ++index)
    {
        if (lower_ascii(extension[index]) != listed[index])
        {
            return false;
        }
    }
    return true;
}
} // namespace

bool folder_lists(std::string_view name) noexcept
{
    const auto extension = extension_of(name);
    if (!extension.has_value() || extension.value().empty())
    {
        return true;
    }
    for (const std::string_view listed : unlisted_extensions)
    {
        if (same_extension(extension.value(), listed))
        {
            return false;
        }
    }
    return true;
}
} // namespace nenenib::core
