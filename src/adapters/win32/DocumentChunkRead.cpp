#include "DocumentChunkRead.hpp"

#include "CompleteFileRead.hpp"
#include "DocumentDecoding.hpp"
#include "DocumentReadFinished.hpp"
#include "DocumentReadProgress.hpp"
#include "FileFailure.hpp"

#include <expected>
#include <new>
#include <span>
#include <utility>

namespace nenenib::adapters::win32
{
DocumentChunkRead::DocumentChunkRead(std::shared_ptr<DocumentReadShelf> shelf, Win32Worker &worker,
                                     application::CodePagePort &code_pages,
                                     std::unique_ptr<FileHandle> file,
                                     core::BytesReservation reservation, std::uint64_t ticket,
                                     std::uint64_t total_bytes, std::size_t chunk_bytes) noexcept
    : shelf_(std::move(shelf)), worker_(&worker), code_pages_(&code_pages), file_(std::move(file)),
      reservation_(std::move(reservation)), ticket_(ticket), total_bytes_(total_bytes),
      chunk_bytes_(chunk_bytes)
{
}

void DocumentChunkRead::run(const std::stop_token &stop)
{
    if (stop.stop_requested() || !shelf_->wanted(ticket_))
    {
        return;
    }
    const std::span<char> target = reservation_.window(chunk_bytes_);
    const HANDLE handle = file_->get();
    auto operation = [handle](std::span<char> part) noexcept
        -> std::expected<std::size_t, application::FileFailure>
    {
        DWORD done = 0;
        if (ReadFile(handle, part.data(), static_cast<DWORD>(part.size()), &done, nullptr) == 0)
        {
            return std::unexpected(application::FileFailure::unreadable);
        }
        return static_cast<std::size_t>(done);
    };
    // 塊の中の反復と短い読みの扱いは同期の読みと同じ 1 本（ADR 0124 決定 4）。
    const auto read = complete_file_read_into(target, operation);
    if (!read)
    {
        shelf_->finish(application::DocumentReadFinished{ticket_, std::unexpected(read.error())});
        return;
    }
    reservation_.commit(target.size());
    read_bytes_ += target.size();
    shelf_->progress(application::DocumentReadProgress{ticket_, read_bytes_, total_bytes_});
    if (read_bytes_ < total_bytes_ && !target.empty())
    {
        Win32Worker &worker = *worker_;
        worker.post([job = std::move(*this)](const std::stop_token &next) mutable
                    { job.run(next); });
        return;
    }
    finish();
}

// 確保の失敗は仕事の中で受けて値に変える（ADR 0159 の決定 6・#817 の形・core は投げない）。
void DocumentChunkRead::finish()
{
    file_.reset();
    try
    {
        auto opened = application::opened_document_of(*code_pages_, std::move(reservation_).seal());
        shelf_->finish(application::DocumentReadFinished{ticket_, std::move(opened)});
    }
    catch (const std::bad_alloc &)
    {
        shelf_->finish(application::DocumentReadFinished{
            ticket_, std::unexpected(application::FileFailure::insufficient_memory)});
    }
}
} // namespace nenenib::adapters::win32
