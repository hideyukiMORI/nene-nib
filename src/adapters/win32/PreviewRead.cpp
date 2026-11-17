#include "PreviewRead.hpp"

#include "DocumentDecoding.hpp"
#include "FileFailure.hpp"
#include "NativePath.hpp"
#include "PreviewBatch.hpp"
#include "PreviewLines.hpp"
#include "ProjectSearchEntries.hpp"
#include "ProjectSearchRules.hpp"
#include "TextEncoding.hpp"
#include "Utf16.hpp"

#include <windows.h>

#include <algorithm>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using application::PreviewFailure;

// UTF-16 の BOM で始まる本文は NUL を含んでも本文（開く道の判定が読む）。
[[nodiscard]] bool utf16_head(std::string_view head) noexcept
{
    return head.starts_with(core::utf16_le_byte_order_mark()) ||
           head.starts_with(core::utf16_be_byte_order_mark());
}

// 開く道の失敗 → 欄の 1 行の理由。復号の上限だけが too_large で、ほかは読めない。
[[nodiscard]] PreviewFailure failure_of(application::FileFailure failure) noexcept
{
    return failure == application::FileFailure::too_large_to_decode ? PreviewFailure::too_large
                                                                    : PreviewFailure::unreadable;
}
} // namespace

PreviewRead::PreviewRead(application::PreviewRequest request, std::uint64_t generation,
                         std::shared_ptr<PreviewShelf> shelf, PreviewTools tools)
    : request_(std::move(request)), generation_(generation), shelf_(std::move(shelf)), tools_(tools)
{
}

void PreviewRead::run(const std::stop_token &stop)
{
    if (stop.stop_requested() || !shelf_->current(generation_))
    {
        return;
    }
    const auto failed = file_ == nullptr ? open() : read_chunk();
    if (failed.has_value())
    {
        fail(failed.value());
        return;
    }
    if (read_bytes_ < total_bytes_)
    {
        again();
        return;
    }
    finish();
}

std::optional<PreviewFailure> PreviewRead::open()
{
    const auto wide = core::to_utf16(request_.path.text());
    if (!wide || wide.value().empty())
    {
        return PreviewFailure::unreadable;
    }
    // 中身が手元に無いファイルは開く前に属性で断る（取り寄せを起こさない・全文検索と同じ順）。
    const auto native = native_path(wide.value());
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (GetFileAttributesExW(native.c_str(), GetFileExInfoStandard, &attributes) == 0)
    {
        return PreviewFailure::unreadable;
    }
    if (!project_file_read(attributes.dwFileAttributes))
    {
        return PreviewFailure::not_local;
    }
    file_ = std::make_unique<FileHandle>(CreateFileW(
        native.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    BY_HANDLE_FILE_INFORMATION information{};
    if (!file_->valid() || GetFileInformationByHandle(file_->get(), &information) == 0 ||
        (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return PreviewFailure::unreadable;
    }
    total_bytes_ = (std::uint64_t{information.nFileSizeHigh} << 32U) | information.nFileSizeLow;
    if (!core::project_file_fits(total_bytes_))
    {
        return PreviewFailure::too_large;
    }
    try
    {
        reservation_.emplace(
            core::BytesReservation::reserve(static_cast<std::size_t>(total_bytes_)));
    }
    catch (const std::bad_alloc &)
    {
        return PreviewFailure::unreadable;
    }
    return read_chunk();
}

std::optional<PreviewFailure> PreviewRead::read_chunk()
{
    if (total_bytes_ == 0 || !reservation_.has_value())
    {
        return std::nullopt;
    }
    const auto length = static_cast<std::size_t>(
        std::min<std::uint64_t>(core::project_search_chunk_bytes, total_bytes_ - read_bytes_));
    const std::span<char> target = reservation_.value().window(length);
    DWORD done = 0;
    if (ReadFile(file_->get(), target.data(), static_cast<DWORD>(target.size()), &done, nullptr) ==
            0 ||
        done == 0)
    {
        return PreviewFailure::unreadable;
    }
    const std::string_view head(target.data(), done);
    if (read_bytes_ == 0 && !utf16_head(head) && core::project_text_is_binary(head))
    {
        return PreviewFailure::binary;
    }
    reservation_.value().commit(done);
    read_bytes_ += done;
    return std::nullopt;
}

// 判定・復号・窓は同じ worker の上で作る。確保の失敗は仕事の中で受けて値に変える。
void PreviewRead::finish()
{
    file_.reset();
    try
    {
        auto bytes = reservation_.has_value() ? std::move(reservation_).value().seal()
                                              : core::BytesReservation::reserve(0).seal();
        reservation_.reset();
        auto opened = application::opened_document_of(*tools_.second, std::move(bytes));
        if (!opened)
        {
            fail(failure_of(opened.error()));
            return;
        }
        static_cast<void>(shelf_->deliver(
            generation_,
            application::PreviewBatch{
                request_.ticket, application::preview_lines(opened.value().text, request_.anchor,
                                                            request_.language)}));
    }
    catch (const std::bad_alloc &)
    {
        fail(PreviewFailure::unreadable);
    }
}

void PreviewRead::fail(PreviewFailure failure)
{
    file_.reset();
    reservation_.reset();
    static_cast<void>(shelf_->deliver(
        generation_, application::PreviewBatch{request_.ticket, std::unexpected(failure)}));
}

void PreviewRead::again()
{
    Win32Worker &worker = *tools_.first;
    worker.post([job = std::move(*this)](const std::stop_token &next) mutable { job.run(next); });
}
} // namespace nenenib::adapters::win32
