#include "RevealCall.hpp"

#include "RevealFailure.hpp"
#include "Utf16.hpp"

#include <objbase.h>
#include <shlobj.h>
#include <windows.h>

#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::adapters::win32
{
std::expected<std::wstring, application::RevealFailure> reveal_target(std::string_view utf8)
{
    auto wide = core::to_utf16(utf8);
    if (!wide.has_value() || wide.value().empty())
    {
        return std::unexpected(application::RevealFailure::failed);
    }
    return std::move(wide).value();
}

application::RevealFailure reveal_failure_of(HRESULT result) noexcept
{
    const bool missing = result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
                         result == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND) ||
                         result == HRESULT_FROM_WIN32(ERROR_BAD_NETPATH) ||
                         result == HRESULT_FROM_WIN32(ERROR_BAD_NET_NAME);
    return missing ? application::RevealFailure::not_found : application::RevealFailure::failed;
}

namespace
{
// STA の中で名前を解き、フォルダを開いて選ぶ。名前の解きに失敗すれば開かない。
[[nodiscard]] std::expected<void, application::RevealFailure>
reveal_in_apartment(const std::wstring &path)
{
    PIDLIST_ABSOLUTE item = nullptr;
    const HRESULT parsed = SHParseDisplayName(path.c_str(), nullptr, &item, 0, nullptr);
    if (FAILED(parsed) || item == nullptr)
    {
        return std::unexpected(reveal_failure_of(parsed));
    }
    const HRESULT opened = SHOpenFolderAndSelectItems(item, 0, nullptr, 0);
    CoTaskMemFree(item);
    if (FAILED(opened))
    {
        return std::unexpected(reveal_failure_of(opened));
    }
    return {};
}
} // namespace

std::expected<void, application::RevealFailure> reveal_in_shell(const std::wstring &path)
{
    const HRESULT apartment =
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(apartment))
    {
        return std::unexpected(application::RevealFailure::failed);
    }
    const auto revealed = reveal_in_apartment(path);
    CoUninitialize();
    return revealed;
}
} // namespace nenenib::adapters::win32
