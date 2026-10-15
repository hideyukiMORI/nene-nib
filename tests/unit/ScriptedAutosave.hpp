#pragma once

#include "AutosaveFailure.hpp"
#include "AutosaveId.hpp"
#include "AutosavePort.hpp"
#include "AutosaveRemnant.hpp"
#include "AutosaveSnapshot.hpp"

#include <cstddef>
#include <expected>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::tests
{
// 自動退避の口の替え玉（ADR 0137 の決定 8）。スレッドは持たない。store と discard
// で受けた頼みを順に 覚え、collect・clear・remnants・read・forget の呼ばれた回数を数え、collect
// が返す失敗を先に 仕込める（仕込んだ分は次の collect が全部を返し、その後は空）。残骸は remain で
// 仕込み（既定は無し）、本文の無い残骸の read は unreadable。forget は残骸の列を変えない。
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
        return remnants_;
    }

    [[nodiscard]] std::expected<std::vector<std::byte>, nenenib::application::AutosaveFailure>
    read(const nenenib::application::AutosaveRemnant &remnant) override
    {
        ++body_reads_;
        const auto found = bodies_.find(remnant.location);
        if (found == bodies_.end())
        {
            return std::unexpected(nenenib::application::AutosaveFailure::unreadable);
        }
        std::vector<std::byte> bytes;
        for (const char unit : found->second)
        {
            bytes.push_back(static_cast<std::byte>(unit));
        }
        return bytes;
    }

    void forget(const nenenib::application::AutosaveRemnant &remnant) override
    {
        forgotten_.push_back(remnant.location);
    }

    // 前回のプロセスが残した退避を 1 本足す。body が無ければ read は失敗する（数えない）。
    void remain(nenenib::application::AutosaveRemnant remnant, std::optional<std::string> body)
    {
        if (body.has_value())
        {
            bodies_.insert_or_assign(remnant.location, std::move(body).value());
        }
        remnants_.push_back(std::move(remnant));
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
    [[nodiscard]] std::size_t body_reads() const noexcept
    {
        return body_reads_;
    }
    // forget を受けた残骸の location（受けた順）。
    [[nodiscard]] const std::vector<std::string> &forgotten() const noexcept
    {
        return forgotten_;
    }
    // ファイルに触れる呼び出しの合計（store・discard・clear・remnants・read・forget）。
    [[nodiscard]] std::size_t touches() const noexcept
    {
        return stored_.size() + discarded_.size() + clears_ + remnant_reads_ + body_reads_ +
               forgotten_.size();
    }

  private:
    std::vector<nenenib::application::AutosaveSnapshot> stored_;
    std::vector<nenenib::application::AutosaveId> discarded_;
    std::vector<nenenib::application::AutosaveFailure> failures_;
    std::size_t collects_ = 0;
    std::size_t clears_ = 0;
    std::size_t remnant_reads_ = 0;
    std::size_t body_reads_ = 0;
    std::vector<nenenib::application::AutosaveRemnant> remnants_;
    std::map<std::string, std::string> bodies_;
    std::vector<std::string> forgotten_;
};
} // namespace nenenib::tests
