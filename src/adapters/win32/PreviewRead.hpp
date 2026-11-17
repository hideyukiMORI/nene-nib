#pragma once

#include "BytesReservation.hpp"
#include "CodePagePort.hpp"
#include "FileHandle.hpp"
#include "PreviewFailure.hpp"
#include "PreviewRequest.hpp"
#include "PreviewShelf.hpp"
#include "Win32Worker.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>

namespace nenenib::adapters::win32
{
// 読みが借りる worker（積み直す先）と文字コードの変換の口（どちらも adapter より長く生きる）。
using PreviewTools = std::pair<Win32Worker *, application::CodePagePort *>;

// 1 つの要求のプレビューの窓の読み（ADR 0189 の決定 4）。worker の上で走り、1 回の run で 1 つの
// 仕事（開いて大きさと属性を見る・塊 core::project_search_chunk_bytes を 1 つ読む・最後に判定と
// 復号と窓）だけをして、残りがあれば自分を worker の列の後ろへ積み直す（FIFO を塞がない・全文検索
// の塊の間に入れる）。
// - 中身がまだ手元に無いファイル（project_file_read が断る属性）は開いた後に読まずに not_local。
// - 16 MiB（core::project_search_file_limit）を超えるファイルは読まずに too_large。
// - 最初の塊に NUL があれば（UTF-16 の BOM で始まる本文は除く）binary。
// - 文字コードの判定と復号は開く道の application::opened_document_of の 1 本（復号の上限を超えれば
//   too_large・判別できなければ unreadable）。窓は application::preview_lines の 1 本。
// - 開けない・読めない・確保できなければ unreadable。
// 止める合図か新しい世代を見たら何も置かずに戻る。やめさせられない限り、batch を必ず 1 つ出す。
// move だけできる（読んでいるファイルのハンドルと予約を 1 つずつ持つ）。
class PreviewRead final
{
  public:
    PreviewRead(application::PreviewRequest request, std::uint64_t generation,
                std::shared_ptr<PreviewShelf> shelf, PreviewTools tools);

    void run(const std::stop_token &stop);

  private:
    // 開いて属性と大きさを見る。読めない理由があればそれ。
    [[nodiscard]] std::optional<application::PreviewFailure> open();
    // 塊を 1 つ読む。読めない理由があればそれ。
    [[nodiscard]] std::optional<application::PreviewFailure> read_chunk();
    void finish();
    void fail(application::PreviewFailure failure);
    void again();

    application::PreviewRequest request_;
    std::uint64_t generation_;
    std::shared_ptr<PreviewShelf> shelf_;
    PreviewTools tools_;
    std::unique_ptr<FileHandle> file_;
    std::optional<core::BytesReservation> reservation_;
    std::uint64_t total_bytes_ = 0;
    std::uint64_t read_bytes_ = 0;
};
} // namespace nenenib::adapters::win32
