#include "Win32DocumentReader.hpp"

#include "BytesReservation.hpp"
#include "DocumentChunkRead.hpp"
#include "DocumentReadFinished.hpp"
#include "FileFailure.hpp"
#include "FileHandle.hpp"
#include "NativePath.hpp"
#include "Utf16.hpp"

#include <windows.h>

#include <expected>
#include <new>
#include <stop_token>
#include <string>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
// 大きさを見られた普通のファイルの大きさ。見られない（無い・フォルダ・経路が変換できない）なら
// 値なしで、同期の道が今の規則で断る。
[[nodiscard]] std::optional<std::uint64_t> plain_file_size(const std::wstring &path)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (path.empty() ||
        GetFileAttributesExW(native_path(path).c_str(), GetFileExInfoStandard, &data) == 0 ||
        (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return std::nullopt;
    }
    return (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32U) | data.nFileSizeLow;
}
} // namespace

Win32DocumentReader::Win32DocumentReader(Win32Worker &worker, application::FilePort &files,
                                         application::CodePagePort &code_pages,
                                         std::size_t synchronous_bytes, std::size_t chunk_bytes)
    : worker_(worker), code_pages_(code_pages), synchronous_(files, code_pages),
      synchronous_bytes_(synchronous_bytes), chunk_bytes_(chunk_bytes == 0 ? 1 : chunk_bytes),
      shelf_(std::make_shared<DocumentReadShelf>())
{
}

// 走っている塊は棚を持ったまま、次の塊で止める合図と知って戻る。合図はもう呼ばない。
Win32DocumentReader::~Win32DocumentReader()
{
    shelf_->close();
}

void Win32DocumentReader::bind(std::function<void()> arrived)
{
    shelf_->bind(std::move(arrived));
}

// 確保の失敗は呼んだスレッドの仕事の中で受けて値に変える（決定 6・#817 の形）。
application::DocumentReadOutcome
Win32DocumentReader::synchronous(const application::DocumentReadRequest &request)
{
    try
    {
        auto outcome = synchronous_.request(request);
        if (outcome.has_value())
        {
            return std::move(outcome).value();
        }
    }
    catch (const std::bad_alloc &)
    {
    }
    return application::DocumentReadFinished{
        request.ticket, std::unexpected(application::FileFailure::insufficient_memory)};
}

std::optional<application::DocumentReadOutcome>
Win32DocumentReader::request(application::DocumentReadRequest request)
{
    const std::wstring wide = core::to_utf16(request.path.text()).value_or(std::wstring{});
    const auto size = plain_file_size(wide);
    if (!size.has_value() || size.value() <= synchronous_bytes_ ||
        size.value() > request.maximum_bytes)
    {
        return synchronous(request);
    }
    auto file = std::make_unique<FileHandle>(CreateFileW(native_path(wide).c_str(), GENERIC_READ,
                                                         FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                                         FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file->valid())
    {
        return synchronous(request);
    }
    // 予約は UI スレッドで 1 回だけ（ADR 0141 の追補 10）。足りなければ頼まずに値で返す。
    std::optional<core::BytesReservation> reservation;
    try
    {
        reservation.emplace(
            core::BytesReservation::reserve(static_cast<std::size_t>(size.value())));
    }
    catch (const std::bad_alloc &)
    {
        return application::DocumentReadFinished{
            request.ticket, std::unexpected(application::FileFailure::insufficient_memory)};
    }
    shelf_->start(request.ticket);
    worker_.post([job = DocumentChunkRead(shelf_, worker_, code_pages_, std::move(file),
                                          std::move(reservation).value(), request.ticket,
                                          size.value(), chunk_bytes_)](
                     const std::stop_token &stop) mutable { job.run(stop); });
    return std::nullopt;
}

void Win32DocumentReader::cancel(std::uint64_t ticket)
{
    shelf_->cancel(ticket);
}

std::vector<application::DocumentReadOutcome> Win32DocumentReader::collect()
{
    return shelf_->take();
}
} // namespace nenenib::adapters::win32
