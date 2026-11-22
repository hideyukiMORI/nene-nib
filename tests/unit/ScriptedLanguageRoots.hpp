#pragma once

#include "FilePath.hpp"
#include "LanguageRootAnswer.hpp"
#include "LanguageRootPort.hpp"
#include "LanguageRootRequest.hpp"
#include "LspRootMarkers.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
// 言語サーバーの root を探す口の替え玉（ADR 0161 の 2026-11-22 の追補・段
// S3）。ファイルもスレッドも 持たない。place
// で「ある」ことにした経路（印のファイル）だけを、本物と同じ core の lsp_root_found
// で探して答える。find の答えは次の collect が返す（hold の間は release まで 留める）。頼みと
// collect の回数を数える。
class ScriptedLanguageRoots final : public nenenib::application::LanguageRootPort
{
  public:
    void find(nenenib::application::LanguageRootRequest request) override
    {
        auto found = nenenib::core::lsp_root_found(
            request.folder, request.ceiling, request.language,
            [this](const nenenib::core::FilePath &candidate) { return present(candidate.text()); });
        requests_.push_back(request);
        auto &queue = held_ ? held_answers_ : answers_;
        queue.push_back(
            nenenib::application::LanguageRootAnswer{std::move(request), std::move(found)});
    }

    [[nodiscard]] std::vector<nenenib::application::LanguageRootAnswer> collect() override
    {
        ++collects_;
        return std::exchange(answers_, {});
    }

    // path（印のファイルかフォルダ）が「ある」ことにする。
    void place(std::string_view path)
    {
        placed_.emplace_back(path);
    }
    // 以後の答えを release まで留める（答えを待つ間の振る舞いを見る）。
    void hold()
    {
        held_ = true;
    }
    void release()
    {
        held_ = false;
        for (auto &answer : held_answers_)
        {
            answers_.push_back(std::move(answer));
        }
        held_answers_.clear();
    }
    [[nodiscard]] const std::vector<nenenib::application::LanguageRootRequest> &
    requests() const noexcept
    {
        return requests_;
    }
    [[nodiscard]] std::size_t collects() const noexcept
    {
        return collects_;
    }
    // port を呼んだ回数（頼み・collect の和・verify_quiet_paths が読む）。
    [[nodiscard]] std::size_t touches() const noexcept
    {
        return requests_.size() + collects_;
    }

  private:
    [[nodiscard]] bool present(std::string_view path) const
    {
        return std::ranges::find(placed_, path) != placed_.end();
    }

    std::vector<std::string> placed_;
    std::vector<nenenib::application::LanguageRootRequest> requests_;
    std::vector<nenenib::application::LanguageRootAnswer> answers_;
    std::vector<nenenib::application::LanguageRootAnswer> held_answers_;
    std::size_t collects_ = 0;
    bool held_ = false;
};
} // namespace nenenib::tests
