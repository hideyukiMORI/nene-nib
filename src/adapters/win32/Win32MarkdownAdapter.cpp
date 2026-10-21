#include "Win32MarkdownAdapter.hpp"

#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"

#include <stop_token>
#include <utility>

namespace nenenib::adapters::win32
{
Win32MarkdownAdapter::Win32MarkdownAdapter(Win32Worker &worker)
    : worker_(worker), shelf_(std::make_shared<MarkdownShelf>())
{
}

// 走っている解析は棚を持ったまま、次の問いで打ち切って戻る。合図はもう呼ばない。
Win32MarkdownAdapter::~Win32MarkdownAdapter()
{
    shelf_->close();
}

void Win32MarkdownAdapter::bind(std::function<void()> arrived)
{
    shelf_->bind(std::move(arrived));
}

// 本文の shared_ptr は仕事が持ち、解析が終わるまで生かす。
void Win32MarkdownAdapter::parse(application::MarkdownRequest request)
{
    shelf_->renew(request.ticket);
    worker_.post(
        [shelf = shelf_, ticket = request.ticket,
         text = std::move(request.text)](const std::stop_token &stop)
        {
            const auto ask = [&] { return stop.stop_requested() || !shelf->wanted(ticket); };
            shelf->deliver(application::MarkdownOutcome{
                ticket, core::parse_markdown(*text, core::MarkdownStop::asking(ask))});
        });
}

std::vector<application::MarkdownOutcome> Win32MarkdownAdapter::collect()
{
    return shelf_->take();
}
} // namespace nenenib::adapters::win32
