#include "Win32StartListAdapter.hpp"

#include "FileBookmarks.hpp"
#include "FileHistory.hpp"

#include <stop_token>
#include <utility>

namespace nenenib::adapters::win32
{
Win32StartListAdapter::Win32StartListAdapter(Win32Worker &worker, application::HistoryPort &history,
                                             application::BookmarkPort &bookmarks)
    : worker_(worker), history_(history), bookmarks_(bookmarks),
      shelf_(std::make_shared<StartListShelf>())
{
}

// 走っている読みは棚を持ったまま終わる。合図はもう呼ばない。
Win32StartListAdapter::~Win32StartListAdapter()
{
    shelf_->close();
}

void Win32StartListAdapter::bind(std::function<void()> arrived)
{
    shelf_->bind(std::move(arrived));
}

// 読めなかった方は空（起動画面はその節を出さない・決定 2）。止める合図が来ていれば読まない。
void Win32StartListAdapter::request()
{
    worker_.post(
        [shelf = shelf_, &history = history_, &bookmarks = bookmarks_](const std::stop_token &stop)
        {
            if (stop.stop_requested())
            {
                return;
            }
            application::StartLists lists;
            lists.recent = history.read().value_or(application::FileHistory{}).files;
            lists.bookmarks = bookmarks.read().value_or(application::FileBookmarks{}).files;
            shelf->deliver(std::move(lists));
        });
}

std::optional<application::StartLists> Win32StartListAdapter::collect()
{
    return shelf_->take();
}
} // namespace nenenib::adapters::win32
