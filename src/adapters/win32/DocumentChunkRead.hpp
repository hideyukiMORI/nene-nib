#pragma once

#include "BytesReservation.hpp"
#include "CodePagePort.hpp"
#include "DocumentReadShelf.hpp"
#include "FileHandle.hpp"
#include "Win32Worker.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stop_token>

namespace nenenib::adapters::win32
{
// 大きなファイルを worker で読む塊の仕事（ADR 0159 の決定 2・3・ADR 0141 の追補 10）。1 回の run で
// 予約の窓を 1 塊ぶん借りて ReadFile で埋め、commit して進み具合を棚に置き、残りがあれば自分を
// worker の列の後ろへ積み直す（FIFO を塞がない・ADR 0062）。最後の塊の後に封印 → 判定 → 本文を
// 同じ worker の上で作って終わりを置く。止める合図か取消を見たら積み直さずに戻り、予約は捨てる。
// move だけできる（予約とハンドルを 1 つずつ持つ）。
class DocumentChunkRead final
{
  public:
    DocumentChunkRead(std::shared_ptr<DocumentReadShelf> shelf, Win32Worker &worker,
                      application::CodePagePort &code_pages, std::unique_ptr<FileHandle> file,
                      core::BytesReservation reservation, std::uint64_t ticket,
                      std::uint64_t total_bytes, std::size_t chunk_bytes) noexcept;

    void run(const std::stop_token &stop);

  private:
    void release() noexcept;
    void finish();

    std::shared_ptr<DocumentReadShelf> shelf_;
    Win32Worker *worker_;
    application::CodePagePort *code_pages_;
    std::unique_ptr<FileHandle> file_;
    core::BytesReservation reservation_;
    std::uint64_t ticket_;
    std::uint64_t total_bytes_;
    std::uint64_t read_bytes_ = 0;
    std::size_t chunk_bytes_;
};
} // namespace nenenib::adapters::win32
