#pragma once

#include "FileBookmarks.hpp"
#include "FileBookmarksFailure.hpp"

#include <expected>

namespace nenenib::application
{
// 明示登録を読み書きする口。ファイルなしだけ空で、読込失敗は上書きしない（ADR 0063）。
class BookmarkPort
{
  public:
    BookmarkPort() = default;
    virtual ~BookmarkPort() = default;
    BookmarkPort(const BookmarkPort &) = delete;
    BookmarkPort(BookmarkPort &&) = delete;
    BookmarkPort &operator=(const BookmarkPort &) = delete;
    BookmarkPort &operator=(BookmarkPort &&) = delete;

    [[nodiscard]] virtual std::expected<FileBookmarks, FileBookmarksFailure> read() = 0;
    [[nodiscard]] virtual std::expected<void, FileBookmarksFailure>
    write(const FileBookmarks &bookmarks) = 0;
};
} // namespace nenenib::application
