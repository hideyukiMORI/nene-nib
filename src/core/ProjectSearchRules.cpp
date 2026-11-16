#include "ProjectSearchRules.hpp"

#include <algorithm>

namespace nenenib::core
{
namespace
{
[[nodiscard]] constexpr char ascii_lower(char byte) noexcept
{
    return byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte - 'A' + 'a') : byte;
}
} // namespace

bool project_folder_skipped(std::string_view name) noexcept
{
    return std::ranges::any_of(
        project_skipped_folders, [name](std::string_view skipped)
        { return std::ranges::equal(name, skipped, {}, ascii_lower, ascii_lower); });
}

bool project_file_fits(std::uint64_t bytes) noexcept
{
    return bytes <= project_search_file_limit;
}

bool project_text_is_binary(std::string_view head) noexcept
{
    // char_traits の検索は memchr を core の外へ出すので std の検索で引く（ARC-003）。
    const std::string_view probe = head.substr(0, project_search_binary_probe);
    return std::ranges::find(probe, '\0') != probe.end();
}
} // namespace nenenib::core
