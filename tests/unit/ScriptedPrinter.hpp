#pragma once

#include "PrintFailure.hpp"
#include "PrintJob.hpp"
#include "PrintOutcome.hpp"
#include "PrintPort.hpp"

#include <expected>
#include <vector>

namespace nenenib::tests
{
// 印刷の替え玉（ADR 0177 の決定 8）。プリンタにもダイアログにも触れず、受けた依頼を順に覚えて、
// 試験が置いた答えを返す。呼ばれた回数は jobs().size()。
class ScriptedPrinter final : public nenenib::application::PrintPort
{
  public:
    using Answer =
        std::expected<nenenib::application::PrintOutcome, nenenib::application::PrintFailure>;

    [[nodiscard]] Answer print(nenenib::application::PrintJob job) override
    {
        jobs_.push_back(std::move(job));
        return answer_;
    }

    void script(Answer answer) noexcept
    {
        answer_ = answer;
    }

    [[nodiscard]] const std::vector<nenenib::application::PrintJob> &jobs() const noexcept
    {
        return jobs_;
    }

  private:
    Answer answer_{nenenib::application::PrintOutcome::printed};
    std::vector<nenenib::application::PrintJob> jobs_;
};
} // namespace nenenib::tests
