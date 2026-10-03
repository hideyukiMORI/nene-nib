#pragma once

#include "BookmarkPort.hpp"
#include "FilePort.hpp"

#include <expected>

namespace nenenib::adapters::win32
{
// 明示登録したブックマークの置き場（ADR 0063 の決定 4）。設定と同じ親の bookmarks.v1。
[[nodiscard]] std::expected<core::FilePath, application::FileBookmarksFailure>
local_bookmark_path();

// bookmarks.v1 を FilePort で読み書きする。lock も「外で変わっていたら上書きしない」も持たない。
// ほかの窓が書いた分は、書く側が読んでから記録して書くことで残す（ADR 0063 の決定 4）。
class Win32BookmarkAdapter final : public application::BookmarkPort
{
  public:
    Win32BookmarkAdapter(application::FilePort &files,
                         std::expected<core::FilePath, application::FileBookmarksFailure> path);
    [[nodiscard]] std::expected<application::FileBookmarks, application::FileBookmarksFailure>
    read() override;
    [[nodiscard]] std::expected<void, application::FileBookmarksFailure>
    write(const application::FileBookmarks &bookmarks) override;

  private:
    application::FilePort &files_;
    std::expected<core::FilePath, application::FileBookmarksFailure> path_;
};
} // namespace nenenib::adapters::win32
