#pragma once

#include "StartListPort.hpp"
#include "StartLists.hpp"

#include <cstddef>
#include <optional>
#include <utility>

namespace nenenib::tests
{
// 起動画面の読みの口の替え玉（ADR 0172 の決定 3）。スレッドは持たず、ファイルも読まない。request
// と collect の呼ばれた回数を数え、collect が返す結果を先に仕込める（仕込んだ分は次の collect が
// 返し、その後は値なし）。
class ScriptedStartLists final : public nenenib::application::StartListPort
{
  public:
    void request() override
    {
        ++requests_;
    }

    [[nodiscard]] std::optional<nenenib::application::StartLists> collect() override
    {
        ++collects_;
        return std::exchange(lists_, std::nullopt);
    }

    // 次の collect が返す結果を置く（数えない）。
    void serve(nenenib::application::StartLists lists)
    {
        lists_ = std::move(lists);
    }
    [[nodiscard]] std::size_t requests() const noexcept
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
        return requests_ + collects_;
    }

  private:
    std::optional<nenenib::application::StartLists> lists_;
    std::size_t requests_ = 0;
    std::size_t collects_ = 0;
};
} // namespace nenenib::tests
