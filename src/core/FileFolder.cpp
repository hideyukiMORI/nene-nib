#include "FileFolder.hpp"

#include <cstddef>
#include <string_view>

namespace nenenib::core
{
namespace
{
// 最後の区切りの位置。区切りが無ければ無し（区切りは 2 種類なので STL の検索は使わない）。
[[nodiscard]] std::optional<std::size_t> last_separator(std::string_view text) noexcept
{
    for (std::size_t index = text.size(); index > 0; --index)
    {
        if (text[index - 1] == '\\' || text[index - 1] == '/')
        {
            return index - 1;
        }
    }
    return std::nullopt;
}

// 区切りの前がルートの印だけか（空か、ドライブの `X:`）。そのときは区切りを残す。
[[nodiscard]] bool root_only(std::string_view before) noexcept
{
    return before.empty() || (before.size() == 2 && before[1] == ':');
}
} // namespace

std::optional<FilePath> folder_of(const FilePath &path)
{
    const std::string_view text = path.text();
    const auto separator = last_separator(text);
    if (!separator.has_value())
    {
        return std::nullopt;
    }
    const std::string_view before = text.substr(0, separator.value());
    // 区切りは ASCII なので、前の部分も検証済みの UTF-8 で制御文字を含まない。
    return FilePath::parse(root_only(before) ? text.substr(0, separator.value() + 1) : before)
        .value();
}
} // namespace nenenib::core
