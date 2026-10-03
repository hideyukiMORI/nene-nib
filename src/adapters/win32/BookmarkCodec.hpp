#pragma once

#include "FileBookmarkEdit.hpp"
#include "FileBookmarks.hpp"
#include "FileBookmarksFailure.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
// bookmarks.v1 の上限（ADR 0063 の決定 4）。越えたものは書かず、読むときも全体を拒む。行の上限も
// 登録の上限と同じ。上限を超えるものを一部だけ採用して書き戻さない。
inline constexpr std::size_t maximum_bookmark_bytes = 1024U * 1024U;
inline constexpr std::size_t maximum_bookmark_lines = application::bookmark_limit;

// 形式（UTF-8・書きは LF・読みは CRLF と BOM も受ける）:
//   version=1
//   <パス>   ← 1 行 1 ファイル・登録順
// 空行・絶対でないパス・FilePath が拒むものは、一部だけ返さず全体を拒む。
[[nodiscard]] std::expected<application::FileBookmarks, application::FileBookmarksFailure>
decode_bookmarks(std::string_view bytes);
[[nodiscard]] std::expected<std::string, application::FileBookmarksFailure>
encode_bookmarks(const application::FileBookmarks &bookmarks);
} // namespace nenenib::adapters::win32
