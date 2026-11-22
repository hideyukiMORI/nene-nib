#include "Win32LanguageRootAdapter.hpp"

#include "LspRootMarkers.hpp"
#include "Utf16.hpp"

#include <windows.h>

#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
#include <vector>

namespace nenenib::adapters::win32
{
std::optional<core::FilePath> language_root_on_disk(const application::LanguageRootRequest &request,
                                                    const std::stop_token &stop)
{
    return core::lsp_root_found(request.folder, request.ceiling, request.language,
                                [&stop](const core::FilePath &candidate)
                                {
                                    if (stop.stop_requested())
                                    {
                                        return false;
                                    }
                                    const auto wide = core::to_utf16(candidate.text());
                                    return wide.has_value() &&
                                           GetFileAttributesW(wide.value().c_str()) !=
                                               INVALID_FILE_ATTRIBUTES;
                                });
}

Win32LanguageRootAdapter::Win32LanguageRootAdapter(Win32Worker &worker)
    : worker_(worker), shelf_(std::make_shared<LanguageRootShelf>())
{
}

// 走っている仕事は棚を持ったまま終わる。合図はもう呼ばない。
Win32LanguageRootAdapter::~Win32LanguageRootAdapter()
{
    shelf_->close();
}

void Win32LanguageRootAdapter::bind(std::function<void()> arrived)
{
    shelf_->bind(std::move(arrived));
}

// 止める合図が来ていれば答えを置かない（窓が閉じていく）。
void Win32LanguageRootAdapter::find(application::LanguageRootRequest request)
{
    worker_.post(
        [shelf = shelf_, request = std::move(request)](const std::stop_token &stop)
        {
            auto found = language_root_on_disk(request, stop);
            if (stop.stop_requested())
            {
                return;
            }
            shelf->deliver(application::LanguageRootAnswer{request, std::move(found)});
        });
}

std::vector<application::LanguageRootAnswer> Win32LanguageRootAdapter::collect()
{
    return shelf_->take();
}
} // namespace nenenib::adapters::win32
