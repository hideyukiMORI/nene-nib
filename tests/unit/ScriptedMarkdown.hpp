#pragma once

#include "MarkdownOutcome.hpp"
#include "MarkdownPort.hpp"
#include "MarkdownRequest.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace nenenib::tests
{
// Markdown を裏で解析する口の替え玉（ADR 0155 の決定 5）。スレッドは持たず、解析もしない。
// parse で受けた要求を順に覚え、collect の呼ばれた回数を数え、collect が返す結果を先に仕込める
// （仕込んだ分は次の collect が全部を返し、その後は空）。
class ScriptedMarkdown final : public nenenib::application::MarkdownPort
{
  public:
    void parse(nenenib::application::MarkdownRequest request) override
    {
        requests_.push_back(std::move(request));
    }

    [[nodiscard]] std::vector<nenenib::application::MarkdownOutcome> collect() override
    {
        ++collects_;
        return std::exchange(outcomes_, {});
    }

    // 次の collect が返す結果を足す（数えない）。
    void serve(nenenib::application::MarkdownOutcome outcome)
    {
        outcomes_.push_back(std::move(outcome));
    }
    [[nodiscard]] const std::vector<nenenib::application::MarkdownRequest> &
    requests() const noexcept
    {
        return requests_;
    }
    [[nodiscard]] std::size_t collects() const noexcept
    {
        return collects_;
    }

  private:
    std::vector<nenenib::application::MarkdownRequest> requests_;
    std::vector<nenenib::application::MarkdownOutcome> outcomes_;
    std::size_t collects_ = 0;
};
} // namespace nenenib::tests
