#include "FileHistoryEdit.hpp"

#include <utility>
#include <vector>

namespace nenenib::application
{
FileHistory history_forgotten(FileHistory history, const core::FilePath &path,
                              const FilePort &files)
{
    std::erase_if(history.files, [&path, &files](const core::FilePath &kept)
                  { return files.same_file(kept, path); });
    return history;
}

FileHistory history_recorded(FileHistory history, const core::FilePath &path, const FilePort &files)
{
    FileHistory rest = history_forgotten(std::move(history), path, files);
    FileHistory recorded;
    recorded.files.reserve(history_limit);
    recorded.files.push_back(path);
    for (core::FilePath &kept : rest.files)
    {
        if (recorded.files.size() == history_limit)
        {
            break;
        }
        recorded.files.push_back(std::move(kept));
    }
    return recorded;
}
} // namespace nenenib::application
