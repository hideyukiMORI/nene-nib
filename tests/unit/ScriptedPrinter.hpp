#pragma once

#include "PrintFailure.hpp"
#include "PrintJob.hpp"
#include "PrintOutcome.hpp"
#include "PrintPort.hpp"
#include "PrintSettings.hpp"

#include <expected>
#include <vector>

namespace nenenib::tests
{
// 印刷の替え玉（ADR 0177 の決定 8・10）。プリンタに触れず、受けた依頼と設定を順に覚えて、試験が
// 置いた答えを返す。呼ばれた回数は jobs().size()。
class ScriptedPrinter final : public nenenib::application::PrintPort
{
  public:
    using Answer =
        std::expected<nenenib::application::PrintOutcome, nenenib::application::PrintFailure>;

    [[nodiscard]] Answer print(const nenenib::application::PrintJob &job,
                               const nenenib::application::PrintSettings &settings) override
    {
        jobs_.push_back(job);
        settings_.push_back(settings);
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

    [[nodiscard]] const std::vector<nenenib::application::PrintSettings> &settings() const noexcept
    {
        return settings_;
    }

  private:
    Answer answer_{nenenib::application::PrintOutcome::printed};
    std::vector<nenenib::application::PrintJob> jobs_;
    std::vector<nenenib::application::PrintSettings> settings_;
};
} // namespace nenenib::tests
