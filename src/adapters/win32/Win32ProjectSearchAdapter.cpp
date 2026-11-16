#include "Win32ProjectSearchAdapter.hpp"

#include "ProjectSearchWalk.hpp"

#include <stop_token>
#include <utility>

namespace nenenib::adapters::win32
{
Win32ProjectSearchAdapter::Win32ProjectSearchAdapter(Win32Worker &worker)
    : worker_(worker), shelf_(std::make_shared<ProjectSearchShelf>())
{
}

// 走っている歩きは棚を持ったまま、次の仕事で古い世代と知って戻る。合図はもう呼ばない。
Win32ProjectSearchAdapter::~Win32ProjectSearchAdapter()
{
    shelf_->close();
}

void Win32ProjectSearchAdapter::bind(std::function<void()> arrived)
{
    shelf_->bind(std::move(arrived));
}

void Win32ProjectSearchAdapter::request(application::ProjectSearchRequest request)
{
    const auto generation = shelf_->renew(request.ticket);
    worker_.post([walk = ProjectSearchWalk(std::move(request), generation, shelf_, worker_)](
                     const std::stop_token &stop) mutable { walk.run(stop); });
}

void Win32ProjectSearchAdapter::cancel(std::uint64_t ticket)
{
    shelf_->cancel(ticket);
}

std::vector<application::ProjectSearchBatch> Win32ProjectSearchAdapter::collect()
{
    return shelf_->take();
}
} // namespace nenenib::adapters::win32
