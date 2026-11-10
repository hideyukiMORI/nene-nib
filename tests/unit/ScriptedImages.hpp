#pragma once

#include "ImageOutcome.hpp"
#include "ImagePort.hpp"
#include "ImageRequest.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace nenenib::tests
{
// プレビューの画像を読む口の替え玉（ADR 0155 の追補 53・#1381）。スレッドは持たず、ファイルも読まな
// い。request で受けた要求を順に覚え、collect の呼ばれた回数を数え、collect が返す結果を先に仕込め
// る（仕込んだ分は次の collect が全部を返し、その後は空）。
class ScriptedImages final : public nenenib::application::ImagePort
{
  public:
    void request(nenenib::application::ImageRequest request) override
    {
        requests_.push_back(std::move(request));
    }

    [[nodiscard]] std::vector<nenenib::application::ImageOutcome> collect() override
    {
        ++collects_;
        return std::exchange(outcomes_, {});
    }

    // 次の collect が返す結果を足す（数えない）。
    void serve(nenenib::application::ImageOutcome outcome)
    {
        outcomes_.push_back(std::move(outcome));
    }
    [[nodiscard]] const std::vector<nenenib::application::ImageRequest> &requests() const noexcept
    {
        return requests_;
    }
    [[nodiscard]] std::size_t collects() const noexcept
    {
        return collects_;
    }
    // 口が呼ばれた回数の和（verify_quiet_paths）。
    [[nodiscard]] std::size_t touches() const noexcept
    {
        return requests_.size() + collects_;
    }

  private:
    std::vector<nenenib::application::ImageRequest> requests_;
    std::vector<nenenib::application::ImageOutcome> outcomes_;
    std::size_t collects_ = 0;
};
} // namespace nenenib::tests
