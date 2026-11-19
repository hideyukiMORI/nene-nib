#include "Win32LinkOpenAdapter.hpp"

#include "LinkOpenCall.hpp"

#include <memory>
#include <stop_token>
#include <string>
#include <utility>

namespace nenenib::adapters::win32
{
Win32LinkOpenAdapter::Win32LinkOpenAdapter(Win32Worker &worker)
    : worker_(worker), shelf_(std::make_shared<LinkOpenShelf>())
{
}

// 走っている仕事は棚を持ったまま終わる。合図はもう呼ばない。
Win32LinkOpenAdapter::~Win32LinkOpenAdapter()
{
    shelf_->close();
}

void Win32LinkOpenAdapter::bind(std::function<void()> arrived)
{
    shelf_->bind(std::move(arrived));
}

// 入口の守りで断ったらシェルを呼ばずに失敗を置く（合図は同じ道で届く）。止める合図が来ていれば
// 呼ばない。
void Win32LinkOpenAdapter::open(std::string uri)
{
    auto target = link_open_target(uri);
    worker_.post(
        [shelf = shelf_, target = std::move(target)](const std::stop_token &stop)
        {
            if (stop.stop_requested())
            {
                return;
            }
            shelf->deliver(
                target.and_then([](const std::wstring &wide) { return open_link_in_shell(wide); }));
        });
}

std::vector<std::expected<void, application::LinkOpenFailure>> Win32LinkOpenAdapter::collect()
{
    return shelf_->take();
}
} // namespace nenenib::adapters::win32
