#pragma once

#include "FilePath.hpp"
#include "FolderEntry.hpp"
#include "FolderProgress.hpp"
#include "FolderRequest.hpp"
#include "FolderShelf.hpp"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stop_token>
#include <vector>

namespace nenenib::adapters::win32
{
// 1 つの要求の列挙（ADR 0062 の決定 8）。ワーカーの上で走り、
// 受け取った要求の値だけを読んで、結果を新しい batch の値として棚へ置く（ADR 0004）。
// 要求が files なら、フォルダの直下のファイルだけを OS が返す順に出す。フォルダ・隠し属性・
// システム属性・UTF-8 にできない名前・FilePath にできない名前は飛ばす。entries なら（ADR 0162 の
// 決定 5）、`.` `..` を除く直下の項目をフォルダか・隠しかと一緒に出す（上限は entry_limit）。
// ファイルを 1 つ読むたびに止める合図と世代を見て、変わっていたら何も出さずに戻る。
// やめさせられない限り、最後の batch（complete・truncated・failed）を必ず 1 つ出す。
class FolderListing final
{
  public:
    FolderListing(application::FolderRequest request, std::uint64_t generation,
                  std::shared_ptr<FolderShelf> shelf);

    void run(const std::stop_token &stop);

  private:
    [[nodiscard]] bool live(const std::stop_token &stop) const;
    // 見えた 1 件を要求の種類で写して足す。列挙を続けるなら true。
    [[nodiscard]] bool listed(const WIN32_FIND_DATAW &data);
    [[nodiscard]] std::size_t limit() const noexcept;
    // 1 件を足す。列挙を続けるなら true（上限を超えた・世代が古いなら false）。
    [[nodiscard]] bool take(core::FilePath file);
    [[nodiscard]] bool take(application::FolderEntry entry);
    // 1 件を足す前の上限と満ちた batch の送り。足してよいなら true。
    [[nodiscard]] bool room();
    [[nodiscard]] bool send(application::FolderProgress progress);
    void finish(const std::stop_token &stop, application::FolderProgress progress);

    application::FolderRequest request_;
    std::uint64_t generation_;
    std::shared_ptr<FolderShelf> shelf_;
    std::vector<core::FilePath> files_;
    std::vector<application::FolderEntry> entries_;
    std::size_t total_ = 0;
};
} // namespace nenenib::adapters::win32
