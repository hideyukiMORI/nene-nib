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

std::expected<SessionPaths, Failure> local_session_paths()
{
    auto current = beside_local_settings("session.v2");
    auto legacy = beside_local_settings("session.v1");
    if (!current.has_value() || !legacy.has_value())
    {
        return std::unexpected(Failure::location_unavailable);
    }
    return SessionPaths{std::move(current).value(), std::move(legacy).value()};
}

Win32SessionAdapter::Win32SessionAdapter(application::FilePort &files,
                                         std::expected<SessionPaths, Failure> paths)
    : files_(files), paths_(std::move(paths))
{
}

std::expected<std::optional<application::Session>, Failure> Win32SessionAdapter::read()
{
    if (!paths_)
    {
        return std::unexpected(paths_.error());
    }
    auto bytes = files_.read(paths_.value().current, maximum_session_bytes);
    if (!bytes && bytes.error() == application::FileFailure::not_found)
    {
        bytes = files_.read(paths_.value().legacy, maximum_session_bytes);
    }
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
    auto session = decode_session(bytes.value().view());
    if (!session)
    {
        return std::unexpected(session.error());
    }
    return std::optional<application::Session>{std::move(session).value()};
}

std::expected<void, Failure> Win32SessionAdapter::write(const application::Session &session)
{
    if (!paths_)
    {
        return std::unexpected(paths_.error());
    }
    const auto bytes = encode_session(session);
    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }
    const auto wide = core::to_utf16(paths_.value().current.text());
    if (!wide || !ensure_parent_directory(wide.value()))
    {
        return std::unexpected(Failure::unwritable);
    }
    if (!files_.write(paths_.value().current, bytes.value()))
    {
        return std::unexpected(Failure::unwritable);
    }
    return {};
}
} // namespace nenenib::adapters::win32
