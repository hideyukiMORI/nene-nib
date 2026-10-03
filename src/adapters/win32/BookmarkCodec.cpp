#include "BookmarkCodec.hpp"

#include "AbsolutePath.hpp"
#include "TextLines.hpp"
#include "Utf8.hpp"

#include <utility>
#include <vector>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::FileBookmarksFailure;
using application::FileBookmarks;
using Lines = std::vector<std::string_view>;

[[nodiscard]] std::expected<FileBookmarks, Failure> paths_from(const Lines &lines)
{
    FileBookmarks bookmarks;
    bookmarks.files.reserve(lines.size() - 1);
    for (std::size_t index = 1; index < lines.size(); ++index)
    {
        const std::string_view line = lines.at(index);
        auto path = core::FilePath::parse(line);
        if (!path || !rooted_path_text(line))
        {
            return std::unexpected(Failure::malformed);
        }
        bookmarks.files.push_back(std::move(path).value());
    }
    return bookmarks;
}
} // namespace

std::expected<FileBookmarks, Failure> decode_bookmarks(std::string_view bytes)
{
    if (bytes.size() > maximum_bookmark_bytes)
    {
        return std::unexpected(Failure::too_large);
    }
    if (bytes.starts_with("\xEF\xBB\xBF"))
    {
        bytes.remove_prefix(3);
    }
    const Lines lines = text_lines(bytes);
    const std::string_view head = lines.empty() ? std::string_view{} : lines.front();
    if (!head.starts_with("version="))
    {
        return std::unexpected(Failure::malformed);
    }
    if (head != "version=1")
    {
        return std::unexpected(Failure::unsupported_version);
    }
    if (!core::validate_utf8(bytes))
    {
        return std::unexpected(Failure::malformed);
    }
    if (lines.size() - 1 > maximum_bookmark_lines)
    {
        return std::unexpected(Failure::too_large);
    }
    return paths_from(lines);
}

std::expected<std::string, Failure> encode_bookmarks(const FileBookmarks &bookmarks)
{
    if (bookmarks.files.size() > maximum_bookmark_lines)
    {
        return std::unexpected(Failure::too_large);
    }
    std::string bytes = "version=1\n";
    for (const core::FilePath &path : bookmarks.files)
    {
        if (!rooted_path_text(path.text()))
        {
            return std::unexpected(Failure::malformed);
        }
        bytes += path.text();
        bytes += '\n';
        if (bytes.size() > maximum_bookmark_bytes)
        {
            return std::unexpected(Failure::too_large);
        }
    }
    return bytes;
}
} // namespace nenenib::adapters::win32
