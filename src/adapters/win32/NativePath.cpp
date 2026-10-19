#include "NativePath.hpp"

#include <windows.h>

#include <cstddef>
#include <string_view>
#include <vector>

namespace nenenib::adapters::win32
{
namespace
{
// CreateDirectoryW は 8.3 の名前を足せる長さまでしか作らない（Maximum Path Length Limitation）。
constexpr std::size_t prefixed_from = MAX_PATH - 12;
constexpr std::wstring_view verbatim = L"\\\\?\\";
constexpr std::wstring_view verbatim_unc = L"\\\\?\\UNC\\";
constexpr std::wstring_view device = L"\\\\.\\";
constexpr std::wstring_view unc = L"\\\\";

// `\\?\` は `/`・`.`・`..` を読み替えないので、付ける前に OS の規則で絶対の形にする。
// 失敗は空。
[[nodiscard]] std::wstring full_path(const std::wstring &path)
{
    const DWORD length = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (length == 0)
    {
        return {};
    }
    std::vector<wchar_t> buffer(length, L'\0');
    const DWORD written = GetFullPathNameW(path.c_str(), length, buffer.data(), nullptr);
    if (written == 0 || written >= length)
    {
        return {};
    }
    return {buffer.data(), written};
}
} // namespace

std::wstring native_path(const std::wstring &path)
{
    if (path.starts_with(verbatim) || path.starts_with(device))
    {
        return path;
    }
    // 長さは絶対にした後で比べる。短い相対の経路も、絶対にすると上限を超えることがある。
    const std::wstring full = full_path(path);
    if (full.empty())
    {
        return path;
    }
    if (full.size() < prefixed_from)
    {
        return full;
    }
    if (full.starts_with(unc))
    {
        return std::wstring(verbatim_unc) + full.substr(unc.size());
    }
    return std::wstring(verbatim) + full;
}
} // namespace nenenib::adapters::win32
