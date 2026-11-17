#pragma once

#include "ProjectSearchBatch.hpp"
#include "ProjectSearchPort.hpp"
#include "ProjectSearchRequest.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace nenenib::tests
{
// プロジェクトの全文検索の口の替え玉（ADR 0167 の決定 7）。スレッドもファイルも持たない。
// request と cancel で受けたものを順に覚え、collect の呼ばれた回数を数え、collect が返す batch を
// 先に仕込める（仕込んだ分は次の collect が全部を返し、その後は空）。取り消した券の batch も、
// 仕込めば返す（届いてしまった古い batch を controller が捨てるかを見る）。
class ScriptedProjectSearch final : public nenenib::application::ProjectSearchPort
{
  public:
    void request(nenenib::application::ProjectSearchRequest request) override
    {
        requests_.push_back(std::move(request));
    }

    void cancel(std::uint64_t ticket) override
    {
        cancels_.push_back(ticket);
    }

    [[nodiscard]] std::vector<nenenib::application::ProjectSearchBatch> collect() override
    {
        ++collects_;
        return std::exchange(batches_, {});
    }

    // 次の collect が返す batch を足す（数えない）。
    void serve(nenenib::application::ProjectSearchBatch batch)
    {
        batches_.push_back(std::move(batch));
    }
    [[nodiscard]] const std::vector<nenenib::application::ProjectSearchRequest> &
    requests() const noexcept
    {
        return requests_;
    }
    [[nodiscard]] const std::vector<std::uint64_t> &cancels() const noexcept
    {
        return cancels_;
    }
    [[nodiscard]] std::size_t collects() const noexcept
    {
        return collects_;
    }
    // port を呼んだ回数（要求・取消・collect の和・verify_quiet_paths が読む）。
    [[nodiscard]] std::size_t touches() const noexcept
    {
        return requests_.size() + cancels_.size() + collects_;
    }

  private:
    std::vector<nenenib::application::ProjectSearchRequest> requests_;
    std::vector<std::uint64_t> cancels_;
    std::vector<nenenib::application::ProjectSearchBatch> batches_;
    std::size_t collects_ = 0;
};
} // namespace nenenib::tests
