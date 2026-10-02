#include "FolderShelf.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::adapters::win32
{
FolderShelf::FolderShelf(std::size_t batch_files, std::size_t file_limit) noexcept
    : batch_files_(std::max<std::size_t>(batch_files, 1)), file_limit_(file_limit)
{
}

std::size_t FolderShelf::batch_files() const noexcept
{
    return batch_files_;
}

std::size_t FolderShelf::file_limit() const noexcept
{
    return file_limit_;
}

void FolderShelf::bind(std::function<void()> arrived)
{
    const std::scoped_lock lock(mutex_);
    arrived_ = std::move(arrived);
}

std::uint64_t FolderShelf::renew()
{
    const std::scoped_lock lock(mutex_);
    return ++generation_;
}

void FolderShelf::close()
{
    const std::scoped_lock lock(mutex_);
    ++generation_;
    arrived_ = nullptr;
}

bool FolderShelf::current(std::uint64_t generation) const
{
    const std::scoped_lock lock(mutex_);
    return generation == generation_;
}

bool FolderShelf::deliver(std::uint64_t generation, application::FolderBatch batch)
{
    std::function<void()> arrived;
    {
        const std::scoped_lock lock(mutex_);
        if (generation != generation_)
        {
            return false;
        }
        if (batches_.empty())
        {
            arrived = arrived_;
        }
        batches_.push_back(std::move(batch));
    }
    // ロックの外で呼ぶ。合図の中から list や collect が呼ばれても止まらない。
    if (arrived)
    {
        arrived();
    }
    return true;
}

std::vector<application::FolderBatch> FolderShelf::take()
{
    const std::scoped_lock lock(mutex_);
    return std::exchange(batches_, {});
}
} // namespace nenenib::adapters::win32
