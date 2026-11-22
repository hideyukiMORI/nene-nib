#include "Win32LanguageServerSearch.hpp"

#include "ServerSearch.hpp"

#include <stop_token>
#include <utility>

namespace nenenib::adapters::win32
{
Win32LanguageServerSearch::Win32LanguageServerSearch(Win32Worker &worker,
                                                     std::function<ServerSearchPlaces()> places)
    : worker_(worker),
      places_(std::make_shared<std::function<ServerSearchPlaces()>>(std::move(places))),
      shelf_(std::make_shared<ServerSearchShelf>())
{
}

// 走っている探しは棚を持ったまま終わる。合図はもう呼ばない。
Win32LanguageServerSearch::~Win32LanguageServerSearch()
{
    shelf_->close();
}

void Win32LanguageServerSearch::bind(std::function<void()> arrived)
{
    shelf_->bind(std::move(arrived));
}

// 止める合図が来ていれば探さない。
void Win32LanguageServerSearch::search(std::vector<application::LanguageServerProbe> probes)
{
    worker_.post(
        [shelf = shelf_, places = places_, probes = std::move(probes)](const std::stop_token &stop)
        {
            if (stop.stop_requested())
            {
                return;
            }
            shelf->deliver(find_language_servers(probes, (*places)()));
        });
}

std::optional<std::vector<std::optional<application::FoundLanguageServer>>>
Win32LanguageServerSearch::collect()
{
    return shelf_->take();
}
} // namespace nenenib::adapters::win32
