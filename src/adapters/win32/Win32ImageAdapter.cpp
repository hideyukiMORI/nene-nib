#include "Win32ImageAdapter.hpp"

#include "FilePath.hpp"
#include "ImageDecode.hpp"
#include "ImageFailure.hpp"
#include "ImageFileRead.hpp"
#include "ImageFormat.hpp"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <new>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::ImageFailure;

// 1 つの要求の結果（ワーカーの仕事の中）。記憶が足りなければ insufficient_memory で、例外を外へ出さ
// ない（Win32Worker::run は noexcept）。
[[nodiscard]] application::ImageOutcome
outcome_of(const application::ImageRequest &request) noexcept
{
    try
    {
        auto bytes = image_file_bytes(request.path);
        if (!bytes.has_value())
        {
            return application::ImageOutcome{request.ticket, std::unexpected(bytes.error())};
        }
        std::string &read = bytes.value();
        const std::string_view head(read.data(),
                                    std::min(read.size(), core::image_format_probe_bytes));
        return application::ImageOutcome{
            request.ticket, decoded_image(std::span<char>(read), core::image_format_of(head))};
    }
    catch (const std::bad_alloc &)
    {
        return application::ImageOutcome{request.ticket,
                                         std::unexpected(Failure::insufficient_memory)};
    }
}

// 仕事の本体。止める合図の後と adapter が壊れた後（棚が閉じた）は読まずに戻る。
void read_into(ImageShelf &shelf, const application::ImageRequest &request,
               const std::stop_token &stop) noexcept
{
    if (stop.stop_requested() || shelf.closed())
    {
        return;
    }
    shelf.deliver(outcome_of(request));
}
} // namespace

Win32ImageAdapter::Win32ImageAdapter(Win32Worker &worker)
    : worker_(worker), shelf_(std::make_shared<ImageShelf>())
{
}

// 走っている読みは棚を持ったまま最後まで読み、結果は閉じた棚が捨てる。合図はもう呼ばない。
Win32ImageAdapter::~Win32ImageAdapter()
{
    shelf_->close();
}

void Win32ImageAdapter::bind(std::function<void()> arrived)
{
    shelf_->bind(std::move(arrived));
}

// 席を先に取ってから仕事を載せる。載せられなければ（記憶が足りない）取った席に失敗を置く。
void Win32ImageAdapter::request(application::ImageRequest request)
{
    shelf_->reserve();
    const std::uint64_t ticket = request.ticket;
    try
    {
        worker_.post([shelf = shelf_, request = std::move(request)](const std::stop_token &stop)
                     { read_into(*shelf, request, stop); });
    }
    catch (const std::bad_alloc &)
    {
        shelf_->deliver(
            application::ImageOutcome{ticket, std::unexpected(Failure::insufficient_memory)});
    }
}

std::vector<application::ImageOutcome> Win32ImageAdapter::collect()
{
    return shelf_->take();
}
} // namespace nenenib::adapters::win32
