#include "Win32PreviewAdapter.hpp"

#include "PreviewRead.hpp"

#include <stop_token>
#include <utility>

namespace nenenib::adapters::win32
{
Win32PreviewAdapter::Win32PreviewAdapter(Win32Worker &worker, application::CodePagePort &code_pages)
    : worker_(worker), code_pages_(code_pages), shelf_(std::make_shared<PreviewShelf>())
{
}

// 走っている読みは棚を持ったまま、次の仕事で古い世代と知って戻る。合図はもう呼ばない。
Win32PreviewAdapter::~Win32PreviewAdapter()
{
    shelf_->close();
}

void Win32PreviewAdapter::bind(std::function<void()> arrived)
{
    shelf_->bind(std::move(arrived));
}

void Win32PreviewAdapter::request(application::PreviewRequest request)
{
    const auto generation = shelf_->renew(request.ticket);
    worker_.post([read = PreviewRead(std::move(request), generation, shelf_,
                                     PreviewTools{&worker_, &code_pages_})](
                     const std::stop_token &stop) mutable { read.run(stop); });
}

void Win32PreviewAdapter::cancel(std::uint64_t ticket)
{
    shelf_->cancel(ticket);
}

std::vector<application::PreviewBatch> Win32PreviewAdapter::collect()
{
    return shelf_->take();
}
} // namespace nenenib::adapters::win32
