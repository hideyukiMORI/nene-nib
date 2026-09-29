#include "ParentDirectory.hpp"

#include <windows.h>

namespace nenenib::adapters::win32
{
bool ensure_parent_directory(const std::wstring &path)
{
    const auto separator = path.find_last_of(L"/\\");
    if (separator == std::wstring::npos)
    {
        return false;
    }
    const auto parent = path.substr(0, separator);
    return CreateDirectoryW(parent.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}
} // namespace nenenib::adapters::win32
