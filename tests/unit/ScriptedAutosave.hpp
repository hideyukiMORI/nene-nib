#pragma once

#include "AutosaveFailure.hpp"
#include "AutosaveId.hpp"
#include "AutosavePort.hpp"
#include "AutosaveRemnant.hpp"
#include "AutosaveSnapshot.hpp"

#include <cstddef>
#include <expected>
#include <utility>
#include <vector>

namespace nenenib::tests
{
// 自動退避の口の替え玉（ADR 0137 の決定 8）。スレッドは持たない。store と discard
// で受けた頼みを順に 覚え、collect・clear・remnants・read・forget の呼ばれた回数を数え、collect
// が返す失敗を先に 仕込める（仕込んだ分は次の collect が全部を返し、その後は空）。残骸は無い。
class ScriptedAutosave final : public nenenib::application::AutosavePort
{
  public:
    void store(nenenib::application::AutosaveSnapshot snapshot) override
    {
        stored_.push_back(std::move(snapshot));
    }

    void discard(nenenib::application::AutosaveId id) override
    {
        discarded_.push_back(id);
    }

    [[nodiscard]] std::vector<nenenib::application::AutosaveFailure> collect() override
    {
        ++collects_;
        return std::exchange(failures_, {});
    }

    void clear() override
    {
        ++clears_;
    }

    [[nodiscard]] std::expected<std::vector<nenenib::application::AutosaveRemnant>,
                                nenenib::application::AutosaveFailure>
    remnants() override
    {
        ++remnant_reads_;
        return std::vector<nenenib::application::AutosaveRemnant>{};
    }

    [[nodiscard]] std::expected<std::vector<std::byte>, nenenib::application::AutosaveFailure>
    read(const nenenib::application::AutosaveRemnant &) override
    {
        ++body_reads_;
        return std::unexpected(nenenib::application::AutosaveFailure::unreadable);
    }

    void forget(const nenenib::application::AutosaveRemnant &) override
    {
        ++forgets_;
    }

    // 次の collect が返す失敗を足す（数えない）。
    void fail(nenenib::application::AutosaveFailure failure)
    {
        failures_.push_back(failure);
    }
    [[nodiscard]] const std::vector<nenenib::application::AutosaveSnapshot> &stored() const noexcept
    {
        return stored_;
    }
    [[nodiscard]] const std::vector<nenenib::application::AutosaveId> &discarded() const noexcept
    {
        return discarded_;
    }
    [[nodiscard]] std::size_t collects() const noexcept
    {
        return collects_;
    }
    [[nodiscard]] std::size_t clears() const noexcept
    {
        return clears_;
    }
    [[nodiscard]] std::size_t remnant_reads() const noexcept
    {
        return remnant_reads_;
    }
    // ファイルに触れる呼び出しの合計（store・discard・clear・remnants・read・forget）。
    [[nodiscard]] std::size_t touches() const noexcept
    {
        return stored_.size() + discarded_.size() + clears_ + remnant_reads_ + body_reads_ +
               forgets_;
    }

  private:
    std::vector<nenenib::application::AutosaveSnapshot> stored_;
    std::vector<nenenib::application::AutosaveId> discarded_;
    std::vector<nenenib::application::AutosaveFailure> failures_;
    std::size_t collects_ = 0;
    std::size_t clears_ = 0;
    std::size_t remnant_reads_ = 0;
    std::size_t body_reads_ = 0;
    std::size_t forgets_ = 0;
};
} // namespace nenenib::tests
