#include "FileFolder.hpp"

#include "AsciiCase.hpp"

#include <cstddef>
#include <string_view>

namespace nenenib::core
{
namespace
{
[[nodiscard]] constexpr bool separator_of(char value) noexcept
{
    return value == '\\' || value == '/';
}

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

// full の先頭 base.size() 文字が base と同じか（ASCII の大小と `\` `/` を畳む）。full は base より
// 短くない。
[[nodiscard]] bool spelled_alike(std::string_view base, std::string_view full) noexcept
{
    for (std::size_t index = 0; index < base.size(); ++index)
    {
        const char left = base[index];
        const char right = full[index];
        const bool both_separators = separator_of(left) && separator_of(right);
        if (!both_separators && ascii_folded(left) != ascii_folded(right))
        {
            return false;
        }
    }
    return true;
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

std::string_view folder_spelling(const FilePath &folder) noexcept
{
    const std::string_view text = folder.text();
    if (text.empty() || !separator_of(text.back()))
    {
        return text;
    }
    const std::string_view before = text.substr(0, text.size() - 1);
    return root_only(before) ? text : before;
}

bool folder_contains(const FilePath &folder, const FilePath &path) noexcept
{
    const std::string_view base = folder_spelling(folder);
    const std::string_view full = path.text();
    if (base.empty() || full.size() < base.size() || !spelled_alike(base, full))
    {
        return false;
    }
    return full.size() == base.size() || separator_of(base.back()) ||
           separator_of(full[base.size()]);
}

bool same_folder(const FilePath &left, const FilePath &right) noexcept
{
    const std::string_view one = folder_spelling(left);
    const std::string_view other = folder_spelling(right);
    return one.size() == other.size() && spelled_alike(one, other);
}
} // namespace nenenib::core
