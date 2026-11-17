#pragma once

#include "PreviewBatch.hpp"
#include "PreviewPort.hpp"
#include "PreviewRequest.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace nenenib::tests
{
// プレビューの窓の読みの口の替え玉（ADR 0189 の決定 8）。スレッドもファイルも持たない。
// request と cancel で受けたものを順に覚え、collect の呼ばれた回数を数え、collect が返す batch を
// 先に仕込める（仕込んだ分は次の collect が全部を返し、その後は空）。取り消した券の batch も、
// 仕込めば返す（届いてしまった古い batch を controller が捨てるかを見る）。
class ScriptedPreview final : public nenenib::application::PreviewPort
{
  public:
    void request(nenenib::application::PreviewRequest request) override
    {
        requests_.push_back(std::move(request));
    }

    void cancel(std::uint64_t ticket) override
    {
        cancels_.push_back(ticket);
    }

    [[nodiscard]] std::vector<nenenib::application::PreviewBatch> collect() override
    {
        ++collects_;
        return std::exchange(batches_, {});
    }

    // 次の collect が返す batch を足す（数えない）。
    void serve(nenenib::application::PreviewBatch batch)
    {
        batches_.push_back(std::move(batch));
    }
    [[nodiscard]] const std::vector<nenenib::application::PreviewRequest> &requests() const noexcept
    {
        return requests_;
    }
    [[nodiscard]] const std::vector<std::uint64_t> &cancels() const noexcept
    {
        return cancels_;
    }
    // 仕込んだまままだ collect されていない batch の数（棚に残っているか）。
    [[nodiscard]] std::size_t pending() const noexcept
    {
        return batches_.size();
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
    std::vector<nenenib::application::PreviewRequest> requests_;
    std::vector<std::uint64_t> cancels_;
    std::vector<nenenib::application::PreviewBatch> batches_;
    std::size_t collects_ = 0;
};
} // namespace nenenib::tests
