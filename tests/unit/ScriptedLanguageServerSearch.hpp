#pragma once

#include "FoundLanguageServer.hpp"
#include "LanguageServerProbe.hpp"
#include "LanguageServerSearchPort.hpp"

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace nenenib::tests
{
// 言語サーバーを探す口の替え玉（LSP 段
// S4・#1945）。スレッドを持たず、ファイルも環境変数も読まない。 search と collect
// の呼ばれた回数と最後の候補の列を覚え、collect が返す結果を先に仕込める（仕込んだ 分は次の collect
// が返し、その後は値なし）。
class ScriptedLanguageServerSearch final : public nenenib::application::LanguageServerSearchPort
{
  public:
    using Found = std::vector<std::optional<nenenib::application::FoundLanguageServer>>;

    void search(std::vector<nenenib::application::LanguageServerProbe> probes) override
    {
        ++searches_;
        probes_ = std::move(probes);
    }

    [[nodiscard]] std::optional<Found> collect() override
    {
        ++collects_;
        return std::exchange(found_, std::nullopt);
    }

    // 次の collect が返す結果を置く（数えない）。
    void serve(Found found)
    {
        found_ = std::move(found);
    }
    [[nodiscard]] const std::vector<nenenib::application::LanguageServerProbe> &
    probes() const noexcept
    {
        return probes_;
    }
    [[nodiscard]] std::size_t searches() const noexcept
    {
        return searches_;
    }
    [[nodiscard]] std::size_t collects() const noexcept
    {
        return collects_;
    }
    // 口が呼ばれた回数の和（verify_quiet_paths）。
    [[nodiscard]] std::size_t touches() const noexcept
    {
        return searches_ + collects_;
    }

  private:
    std::vector<nenenib::application::LanguageServerProbe> probes_;
    std::optional<Found> found_;
    std::size_t searches_ = 0;
    std::size_t collects_ = 0;
};
} // namespace nenenib::tests
