#include "ProbeOverlappedStorage.hpp"
#include <utility>
namespace nenenib::adapters::win32
{
ProbeOverlappedStorage::ProbeOverlappedStorage(ProbeStoragePermit, std::unique_ptr<FileHandle> file,
                                               std::unique_ptr<FileHandle> event, std::size_t count)
    : file_(std::move(file)), event_(std::move(event)), count_(count)
{
    overlapped_.hEvent = event_->get();
}
std::expected<std::shared_ptr<ProbeOverlappedStorage>, ProbeReadFailure>
ProbeOverlappedStorage::create(std::unique_ptr<FileHandle> file, std::size_t count)
{
    if (count == 0 || count > 64)
    {
        return std::unexpected(ProbeReadFailure::invalid_count);
    }
    if (!file || !file->valid())
    {
        return std::unexpected(ProbeReadFailure::create_failed);
    }
    const auto event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (event == nullptr)
    {
        return std::unexpected(ProbeReadFailure::create_failed);
    }
    return std::make_shared<ProbeOverlappedStorage>(ProbeStoragePermit{}, std::move(file),
                                                    std::make_unique<FileHandle>(event), count);
}
} // namespace nenenib::adapters::win32
