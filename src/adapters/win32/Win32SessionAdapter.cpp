#include "Win32SessionAdapter.hpp"

#include "LocalSettingsPath.hpp"
#include "ParentDirectory.hpp"
#include "SessionCodec.hpp"
#include "Utf16.hpp"

#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::SessionFailure;
} // namespace

std::expected<core::FilePath, Failure> local_session_path()
{
    auto path = beside_local_settings("session.v1");
    if (!path.has_value())
    {
        return std::unexpected(Failure::location_unavailable);
    }
    return std::move(path).value();
}

Win32SessionAdapter::Win32SessionAdapter(application::FilePort &files,
                                         std::expected<core::FilePath, Failure> path)
    : files_(files), path_(std::move(path))
{
}

std::expected<std::optional<application::Session>, Failure> Win32SessionAdapter::read()
{
    if (!path_)
    {
        return std::unexpected(path_.error());
    }
    const auto bytes = files_.read(path_.value(), maximum_session_bytes);
    if (!bytes)
    {
        if (bytes.error() == application::FileFailure::not_found)
        {
            return std::optional<application::Session>{};
        }
        if (bytes.error() == application::FileFailure::too_large)
        {
            return std::unexpected(Failure::too_large);
        }
        return std::unexpected(Failure::unreadable);
    }
    auto session = decode_session(bytes.value());
    if (!session)
    {
        return std::unexpected(session.error());
    }
    return std::optional<application::Session>{std::move(session).value()};
}

std::expected<void, Failure> Win32SessionAdapter::write(const application::Session &session)
{
    if (!path_)
    {
        return std::unexpected(path_.error());
    }
    const auto bytes = encode_session(session);
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
