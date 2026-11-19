#include "Win32RevealAdapter.hpp"

#include "RevealCall.hpp"

#include <memory>
#include <stop_token>
#include <string>
#include <utility>

namespace nenenib::adapters::win32
{
Win32RevealAdapter::Win32RevealAdapter(Win32Worker &worker)
    : worker_(worker), shelf_(std::make_shared<RevealShelf>())
{
}

// 走っている仕事は棚を持ったまま終わる。合図はもう呼ばない。
Win32RevealAdapter::~Win32RevealAdapter()
{
    shelf_->close();
}

void Win32RevealAdapter::bind(std::function<void()> arrived)
{
    shelf_->bind(std::move(arrived));
}

// 経路を作れなければシェルを呼ばずに失敗を置く（合図は同じ道で届く）。止める合図が来ていれば
// 呼ばない。
void Win32RevealAdapter::reveal(core::FilePath path)
{
    auto target = reveal_target(path.text());
    worker_.post(
        [shelf = shelf_, target = std::move(target)](const std::stop_token &stop)
        {
            if (stop.stop_requested())
            {
                return;
            }
            shelf->deliver(
                target.and_then([](const std::wstring &wide) { return reveal_in_shell(wide); }));
        });
}

std::vector<std::expected<void, application::RevealFailure>> Win32RevealAdapter::collect()
{
    return shelf_->take();
}
} // namespace nenenib::adapters::win32
