#pragma once

#include "FilePath.hpp"
#include "RevealFailure.hpp"
#include "RevealPort.hpp"

#include <cstddef>
#include <expected>
#include <utility>
#include <vector>

namespace nenenib::tests
{
// エクスプローラーで場所を開く口の替え玉（ADR 0078 の追補 #1790）。シェルもスレッドも持たない。
// reveal で受けた経路を順に覚え、collect の呼ばれた回数を数え、collect が返す結果を先に仕込める
// （仕込んだ分は次の collect が全部を返し、その後は空）。
class ScriptedReveal final : public nenenib::application::RevealPort
{
  public:
    using Result = std::expected<void, nenenib::application::RevealFailure>;

    void reveal(nenenib::core::FilePath path) override
    {
        paths_.push_back(std::move(path));
    }

    [[nodiscard]] std::vector<Result> collect() override
    {
        ++collects_;
        return std::exchange(results_, {});
    }

    // 次の collect が返す結果を足す（数えない）。
    void serve(Result result)
    {
        results_.push_back(result);
    }
    [[nodiscard]] const std::vector<nenenib::core::FilePath> &paths() const noexcept
    {
        return paths_;
    }
    [[nodiscard]] std::size_t collects() const noexcept
    {
        return collects_;
    }
    // port を呼んだ回数（頼み・collect の和・verify_quiet_paths が読む）。
    [[nodiscard]] std::size_t touches() const noexcept
    {
        return paths_.size() + collects_;
    }

  private:
    std::vector<nenenib::core::FilePath> paths_;
    std::vector<Result> results_;
    std::size_t collects_ = 0;
};
} // namespace nenenib::tests
