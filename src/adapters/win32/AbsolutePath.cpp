#include "AbsolutePath.hpp"

#include <windows.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::adapters::win32
{
namespace
{
[[nodiscard]] std::string narrow(std::wstring_view wide)
{
    const auto units = static_cast<int>(wide.size());
    const int bytes =
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), units, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0)
    {
        return {};
    }
    std::string utf8(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), units, utf8.data(), bytes, nullptr, nullptr);
    return utf8;
}
} // namespace

std::optional<core::FilePath> absolute_file_path(const std::wstring &path)
{
    const DWORD length = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (length == 0)
    {
        return std::nullopt;
    }
    std::vector<wchar_t> buffer(length, L'\0');
    const DWORD written = GetFullPathNameW(path.c_str(), length, buffer.data(), nullptr);
    if (written == 0 || written >= length)
    {
        return std::nullopt;
    }
    auto parsed = core::FilePath::parse(narrow(std::wstring_view(buffer.data(), written)));
    if (!parsed)
    {
        return std::nullopt;
    }
    return std::move(parsed).value();
}
} // namespace nenenib::adapters::win32
