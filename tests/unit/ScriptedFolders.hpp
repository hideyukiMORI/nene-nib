#pragma once

#include "FolderBatch.hpp"
#include "FolderPort.hpp"
#include "FolderRequest.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace nenenib::tests
{
// 同じフォルダを裏で読む口の替え玉（ADR 0062 の決定 10）。スレッドは持たない。
// list で受けた要求を順に覚え、collect の呼ばれた回数を数え、collect が返す batch を先に仕込める
// （仕込んだ分は次の collect が全部を返し、その後は空）。
class ScriptedFolders final : public nenenib::application::FolderPort
{
  public:
    void list(nenenib::application::FolderRequest request) override
    {
        requests_.push_back(std::move(request));
    }

    [[nodiscard]] std::vector<nenenib::application::FolderBatch> collect() override
    {
        ++collects_;
        return std::exchange(batches_, {});
    }

    // 次の collect が返す batch を足す（数えない）。
    void serve(nenenib::application::FolderBatch batch)
    {
        batches_.push_back(std::move(batch));
    }
    [[nodiscard]] const std::vector<nenenib::application::FolderRequest> &requests() const noexcept
    {
        return requests_;
    }
    [[nodiscard]] std::size_t collects() const noexcept
    {
        return collects_;
    }

  private:
    std::vector<nenenib::application::FolderRequest> requests_;
    std::vector<nenenib::application::FolderBatch> batches_;
    std::size_t collects_ = 0;
};
} // namespace nenenib::tests
