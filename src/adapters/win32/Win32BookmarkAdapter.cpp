#include "Win32BookmarkAdapter.hpp"

#include "BookmarkCodec.hpp"
#include "LocalSettingsPath.hpp"
#include "ParentDirectory.hpp"
#include "Utf16.hpp"

#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::FileBookmarksFailure;
} // namespace

std::expected<core::FilePath, Failure> local_bookmark_path()
{
    auto path = beside_local_settings("bookmarks.v1");
    if (!path.has_value())
    {
        return std::unexpected(Failure::location_unavailable);
    }
    return std::move(path).value();
}

Win32BookmarkAdapter::Win32BookmarkAdapter(application::FilePort &files,
                                           std::expected<core::FilePath, Failure> path)
    : files_(files), path_(std::move(path))
{
}

std::expected<application::FileBookmarks, Failure> Win32BookmarkAdapter::read()
{
    if (!path_)
    {
        return std::unexpected(path_.error());
    }
    const auto bytes = files_.read(path_.value(), maximum_bookmark_bytes);
    if (!bytes)
    {
        if (bytes.error() == application::FileFailure::not_found)
        {
            return application::FileBookmarks{};
        }
        if (bytes.error() == application::FileFailure::too_large)
        {
            return std::unexpected(Failure::too_large);
        }
        return std::unexpected(Failure::unreadable);
    }
    return decode_bookmarks(bytes.value());
}

std::expected<void, Failure>
Win32BookmarkAdapter::write(const application::FileBookmarks &bookmarks)
{
    if (!path_)
    {
        return std::unexpected(path_.error());
    }
    const auto bytes = encode_bookmarks(bookmarks);
    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }
    const auto wide = core::to_utf16(path_.value().text());
    if (!wide || !ensure_parent_directory(wide.value()))
    {
        return std::unexpected(Failure::unwritable);
    }
    if (!files_.write(path_.value(), bytes.value()))
    {
        return std::unexpected(Failure::unwritable);
    }
    return {};
}
} // namespace nenenib::adapters::win32
