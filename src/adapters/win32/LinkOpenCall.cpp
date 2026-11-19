#include "LinkOpenCall.hpp"

#include "LinkOpenFailure.hpp"

#include <objbase.h>
#include <shellapi.h>
#include <windows.h>

#include <array>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
namespace
{
constexpr std::array<std::string_view, 3> allowed_schemes{"http:", "https:", "mailto:"};
// RFC 3986 の unreserved（英数字を除く）と reserved と、百分率の符号の `%`。
constexpr std::string_view uri_marks = "-._~:/?#[]@!$&'()*+,;=%";

[[nodiscard]] bool uri_character(char character) noexcept
{
    const bool alphanumeric = (character >= 'a' && character <= 'z') ||
                              (character >= 'A' && character <= 'Z') ||
                              (character >= '0' && character <= '9');
    return alphanumeric || uri_marks.find(character) != std::string_view::npos;
}

[[nodiscard]] char folded(char character) noexcept
{
    return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a')
                                                : character;
}

[[nodiscard]] bool starts_with_folded(std::string_view text, std::string_view head) noexcept
{
    if (text.size() < head.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < head.size(); ++index)
    {
        if (folded(text[index]) != head[index])
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool allowed_scheme(std::string_view uri) noexcept
{
    for (const std::string_view scheme : allowed_schemes)
    {
        if (starts_with_folded(uri, scheme))
        {
            return true;
        }
    }
    return false;
}
} // namespace

std::expected<std::wstring, application::LinkOpenFailure> link_open_target(std::string_view uri)
{
    if (!allowed_scheme(uri))
    {
        return std::unexpected(application::LinkOpenFailure::refused);
    }
    std::wstring wide;
    wide.reserve(uri.size());
    for (const char character : uri)
    {
        if (!uri_character(character))
        {
            return std::unexpected(application::LinkOpenFailure::refused);
        }
        wide.push_back(static_cast<wchar_t>(character));
    }
    return wide;
}

std::expected<void, application::LinkOpenFailure> open_link_in_shell(const std::wstring &uri)
{
    const HRESULT apartment =
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(apartment))
    {
        return std::unexpected(application::LinkOpenFailure::failed);
    }
    SHELLEXECUTEINFOW execute{};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    execute.lpVerb = L"open";
    execute.lpFile = uri.c_str();
    execute.nShow = SW_SHOWNORMAL;
    const BOOL opened = ShellExecuteExW(&execute);
    CoUninitialize();
    if (opened == FALSE)
    {
        return std::unexpected(application::LinkOpenFailure::failed);
    }
    return {};
}
} // namespace nenenib::adapters::win32
