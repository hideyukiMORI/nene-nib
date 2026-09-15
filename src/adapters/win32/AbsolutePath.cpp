#include "AbsolutePath.hpp"

#include "Utf16.hpp"

#include <windows.h>

#include <string>
#include <string_view>
#include <vector>

namespace nenenib::adapters::win32
{
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
    // 変換できない経路（対にならないサロゲート）は空になり、FilePath::parse が拒む。
    auto parsed = core::FilePath::parse(
        core::to_utf8(std::wstring_view(buffer.data(), written)).value_or(std::string{}));
    if (!parsed)
    {
        return std::nullopt;
    }
    return std::move(parsed).value();
}
} // namespace nenenib::adapters::win32
