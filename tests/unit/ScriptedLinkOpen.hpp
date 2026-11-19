#pragma once

#include "LinkOpenFailure.hpp"
#include "LinkOpenPort.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::tests
{
// リンクを OS に渡す口の替え玉（#1869 の段 L2）。シェルもスレッドも持たない。open で受けた綴りを
// 順に覚え、collect の呼ばれた回数を数え、collect が返す結果を先に仕込める（仕込んだ分は次の
// collect が全部を返し、その後は空）。
class ScriptedLinkOpen final : public nenenib::application::LinkOpenPort
{
  public:
    using Result = std::expected<void, nenenib::application::LinkOpenFailure>;

    void open(std::string uri) override
    {
        uris_.push_back(std::move(uri));
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
    [[nodiscard]] const std::vector<std::string> &uris() const noexcept
    {
        return uris_;
    }
    [[nodiscard]] std::size_t collects() const noexcept
    {
        return collects_;
    }
    // port を呼んだ回数（頼み・collect の和・verify_quiet_paths が読む）。
    [[nodiscard]] std::size_t touches() const noexcept
    {
        return uris_.size() + collects_;
    }

  private:
    std::vector<std::string> uris_;
    std::vector<Result> results_;
    std::size_t collects_ = 0;
};
} // namespace nenenib::tests
