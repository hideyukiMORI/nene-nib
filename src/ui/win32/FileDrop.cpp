#include "FileDrop.hpp"

#include "Utf16.hpp"

#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
[[nodiscard]] std::expected<core::FilePath, application::FileFailure> dropped_path(HDROP drop,
                                                                                   UINT index)
{
    const UINT length = DragQueryFileW(drop, index, nullptr, 0);
    if (length == 0 || length == std::numeric_limits<UINT>::max())
    {
        return std::unexpected(application::FileFailure::unreadable);
    }
    std::wstring wide(static_cast<std::size_t>(length) + 1, L'\0');
    if (DragQueryFileW(drop, index, wide.data(), length + 1) != length)
    {
        return std::unexpected(application::FileFailure::unreadable);
    }
    wide.resize(length);
    auto parsed = core::FilePath::parse(core::to_utf8(wide).value_or(std::string{}));
    if (!parsed)
    {
        return std::unexpected(application::FileFailure::unreadable);
    }
    return std::move(parsed).value();
}
} // namespace

std::vector<std::expected<core::FilePath, application::FileFailure>> take_dropped_files(HDROP drop)
{
    const std::unique_ptr<std::remove_pointer_t<HDROP>, decltype(&DragFinish)> owner(drop,
                                                                                     &DragFinish);
    std::vector<std::expected<core::FilePath, application::FileFailure>> paths;
    if (drop == nullptr)
    {
        return paths;
    }
    const UINT count = DragQueryFileW(drop, std::numeric_limits<UINT>::max(), nullptr, 0);
    paths.reserve(count);
    for (UINT index = 0; index < count; ++index)
    {
        paths.push_back(dropped_path(drop, index));
    }
    return paths;
}
} // namespace nenenib::ui::win32
