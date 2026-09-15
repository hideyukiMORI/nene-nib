#include "FilePath.hpp"

#include "Utf8.hpp"

#include <cstddef>
#include <utility>

namespace nenenib::core
{
namespace
{
// 区切りの探索は後ろから前へ 1 度だけ走る。STL の検索を使わないのは、経路の区切りが
// 2 種類あって「最後のどちらか」を探すからである。
[[nodiscard]] std::size_t name_start(std::string_view text) noexcept
{
    std::size_t start = 0;
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (text[index] == '\\' || text[index] == '/')
        {
            start = index + 1;
        }
    }
    return start;
}
} // namespace

FilePath::FilePath(std::string text) : text_(std::move(text)) {}

std::expected<FilePath, TextFailure> FilePath::parse(std::string_view text)
{
    if (text.empty())
    {
        return std::unexpected(TextFailure::empty);
    }
    const auto code_points = validate_utf8(text);
    if (!code_points)
    {
        return std::unexpected(code_points.error());
    }
    if (has_control_character(text))
    {
        return std::unexpected(TextFailure::control_character);
    }
    return FilePath(std::string(text));
}

std::string_view FilePath::text() const noexcept
{
    return text_;
}

std::string_view FilePath::file_name() const noexcept
{
    return std::string_view(text_).substr(name_start(text_));
}

bool operator==(const FilePath &left, const FilePath &right) noexcept
{
    return left.text() == right.text();
}
} // namespace nenenib::core
