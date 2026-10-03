#pragma once

#include "FileBookmarks.hpp"
#include "FileBookmarksFailure.hpp"
#include "FilePort.hpp"

#include <cstddef>
#include <expected>

namespace nenenib::application
{
inline constexpr std::size_t bookmark_limit = 1024;

[[nodiscard]] bool bookmarks_contain(const FileBookmarks &bookmarks, const core::FilePath &path,
                                     const FilePort &files);
// 既にあれば同じファイルをすべて外し、無ければ末尾へ足す。満杯で古い登録を捨てない。
[[nodiscard]] std::expected<FileBookmarks, FileBookmarksFailure>
bookmarks_toggled(FileBookmarks bookmarks, const core::FilePath &path, const FilePort &files);
} // namespace nenenib::application
