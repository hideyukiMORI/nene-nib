#include "Win32FolderAdapter.hpp"

#include "FolderListing.hpp"

#include <stop_token>
#include <utility>

namespace nenenib::adapters::win32
{
Win32FolderAdapter::Win32FolderAdapter(Win32Worker &worker, std::size_t batch_files,
                                       std::size_t file_limit)
    : worker_(worker), shelf_(std::make_shared<FolderShelf>(batch_files, file_limit))
{
}

// 走っている列挙は棚を持ったまま、次の区切りで古い世代と知って戻る。合図はもう呼ばない。
Win32FolderAdapter::~Win32FolderAdapter()
{
    shelf_->close();
}

void Win32FolderAdapter::bind(std::function<void()> arrived)
{
    shelf_->bind(std::move(arrived));
}

void Win32FolderAdapter::list(application::FolderRequest request)
{
    const auto generation = shelf_->renew();
    worker_.post([listing = FolderListing(std::move(request), generation, shelf_)](
                     const std::stop_token &stop) mutable { listing.run(stop); });
}

std::vector<application::FolderBatch> Win32FolderAdapter::collect()
{
    return shelf_->take();
}
} // namespace nenenib::adapters::win32
