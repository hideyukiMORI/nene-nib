#include "ProjectSearchEntries.hpp"

namespace nenenib::adapters::win32
{
bool project_folder_walked(DWORD attributes, DWORD reparse_tag) noexcept
{
    if ((attributes & FILE_ATTRIBUTE_HIDDEN) != 0)
    {
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
    {
        return true;
    }
    return reparse_tag != IO_REPARSE_TAG_MOUNT_POINT && reparse_tag != IO_REPARSE_TAG_SYMLINK;
}

bool project_file_read(DWORD attributes) noexcept
{
    return (attributes & (FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS | FILE_ATTRIBUTE_OFFLINE)) == 0;
}
} // namespace nenenib::adapters::win32
