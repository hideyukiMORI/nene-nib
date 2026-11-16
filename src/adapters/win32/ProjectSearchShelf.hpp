#pragma once

#include "ProjectSearchBatch.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace nenenib::adapters::win32
{
// 全文検索の歩きと UI スレッドが共有する棚（ADR 0167 の決定 4・FolderShelf と同じ形）。届いた
// batch のたまり・今の要求の世代と券・「届いた」の合図を 1 つのロックで守る。歩きの仕事が
// shared_ptr で持つので、adapter が先に壊れても残る。世代は request のたびに進める adapter の中の
// 数で、cancel は今の券のときだけ世代を進める（古い券の取消で今の要求を止めない）。古い世代の
// batch は受け取らない。合図はたまりが空から 1 つ以上になったときだけ、ロックの外で呼ぶ。
class ProjectSearchShelf final
{
  public:
    void bind(std::function<void()> arrived);
    // 券 ticket の新しい要求の世代を返す。前の世代の歩きは以後 batch を出さない。
    [[nodiscard]] std::uint64_t renew(std::uint64_t ticket);
    // 今の要求の券が ticket なら、その歩きを止める（世代を進める）。
    void cancel(std::uint64_t ticket);
    // どの歩きの batch も受け取らず、合図も呼ばない（adapter が壊れるとき）。
    void close();
    [[nodiscard]] bool current(std::uint64_t generation) const;
    // 今の世代なら batch をたまりに置いて true。古い世代なら置かずに false。
    [[nodiscard]] bool deliver(std::uint64_t generation, application::ProjectSearchBatch batch);
    [[nodiscard]] std::vector<application::ProjectSearchBatch> take();

  private:
    mutable std::mutex mutex_;
    std::vector<application::ProjectSearchBatch> batches_;
    std::uint64_t generation_ = 0;
    std::uint64_t ticket_ = 0;
    std::function<void()> arrived_;
};
} // namespace nenenib::adapters::win32
