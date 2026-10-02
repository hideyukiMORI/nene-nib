#pragma once

#include "FolderBatch.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace nenenib::adapters::win32
{
// フォルダの列挙と UI スレッドが共有する棚（ADR 0062 の決定 6 / 8）。
// 読めた batch のたまり・今の列挙の世代・「届いた」の合図を 1 つのロックで守る。
// 列挙の仕事が shared_ptr で持つので、adapter が先に壊れても残る。
// 世代は list のたびに adapter の中で進める数（application の券とは別）。
// 古い世代の batch は受け取らない。
// 合図はたまりが空から 1 つ以上になったときだけ、ロックの外で呼ぶ。
class FolderShelf final
{
  public:
    FolderShelf(std::size_t batch_files, std::size_t file_limit) noexcept;

    [[nodiscard]] std::size_t batch_files() const noexcept;
    [[nodiscard]] std::size_t file_limit() const noexcept;
    void bind(std::function<void()> arrived);
    // 新しい列挙の世代を返す。前の世代の列挙は以後 batch を出さない。
    [[nodiscard]] std::uint64_t renew();
    // どの列挙の batch も受け取らず、合図も呼ばない（adapter が壊れるとき）。
    void close();
    [[nodiscard]] bool current(std::uint64_t generation) const;
    // 今の世代なら batch をたまりに置いて true。古い世代なら置かずに false。
    [[nodiscard]] bool deliver(std::uint64_t generation, application::FolderBatch batch);
    [[nodiscard]] std::vector<application::FolderBatch> take();

  private:
    std::size_t batch_files_;
    std::size_t file_limit_;
    mutable std::mutex mutex_;
    std::vector<application::FolderBatch> batches_;
    std::uint64_t generation_ = 0;
    std::function<void()> arrived_;
};
} // namespace nenenib::adapters::win32
