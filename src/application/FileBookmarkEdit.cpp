#include "FileBookmarkEdit.hpp"

#include <algorithm>

namespace nenenib::application
{
bool bookmarks_contain(const FileBookmarks &bookmarks, const core::FilePath &path,
                       const FilePort &files)
{
    return std::ranges::any_of(bookmarks.files, [&path, &files](const core::FilePath &kept)
                               { return files.same_file(kept, path); });
}

std::expected<FileBookmarks, FileBookmarksFailure>
bookmarks_toggled(FileBookmarks bookmarks, const core::FilePath &path, const FilePort &files)
{
    if (bookmarks_contain(bookmarks, path, files))
    {
        std::erase_if(bookmarks.files, [&path, &files](const core::FilePath &kept)
                      { return files.same_file(kept, path); });
        return bookmarks;
    }
    if (bookmarks.files.size() >= bookmark_limit)
    {
        return std::unexpected(FileBookmarksFailure::too_large);
    }
    bookmarks.files.push_back(path);
    return bookmarks;
}
} // namespace nenenib::application
