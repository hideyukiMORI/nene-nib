#pragma once

#include "CodePagePort.hpp"
#include "DocumentDecoding.hpp"
#include "DocumentReadFinished.hpp"
#include "DocumentReadOutcome.hpp"
#include "DocumentReadPort.hpp"
#include "DocumentReadRequest.hpp"
#include "FilePort.hpp"
#include "OpenedDocument.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace nenenib::tests
{
// 開く道の読みの替え玉（ADR 0159 の決定 10）。スレッドは持たない。読むのは借りた FilePort と
// CodePagePort（ScriptedFiles と ScriptedCodePages）で、製品の同期の道と同じ判定と復号を通す。
// 本文が defer_above を超える要求は揃えた結果を券ごとに預けて値なしを返し、試験が finish で
// 次の collect に載せる（serve で進み具合や任意の結果も積める）。既定は全部を同期に揃える。
class ScriptedDocumentReads final : public nenenib::application::DocumentReadPort
{
  public:
    ScriptedDocumentReads(nenenib::application::FilePort &files,
                          nenenib::application::CodePagePort &code_pages)
        : files_(files), code_pages_(code_pages)
    {
    }

    [[nodiscard]] std::optional<nenenib::application::DocumentReadOutcome>
    request(nenenib::application::DocumentReadRequest request) override
    {
        requests_.push_back(request);
        auto document = nenenib::application::read_opened_document(
            files_, code_pages_, request.path, request.maximum_bytes);
        if (document.has_value() && document.value().text.size_bytes() > defer_above_)
        {
            held_.insert_or_assign(request.ticket, std::move(document));
            return std::nullopt;
        }
        return nenenib::application::DocumentReadFinished{request.ticket, std::move(document)};
    }

    void cancel(std::uint64_t ticket) override
    {
        cancels_.push_back(ticket);
    }

    [[nodiscard]] std::vector<nenenib::application::DocumentReadOutcome> collect() override
    {
        ++collects_;
        return std::exchange(outcomes_, {});
    }

    // 本文がこの大きさ（バイト）を超える要求を裏へ回す。
    void defer_above(std::size_t bytes) noexcept
    {
        defer_above_ = bytes;
    }
    // 次の collect が返す値を足す（数えない）。
    void serve(nenenib::application::DocumentReadOutcome outcome)
    {
        outcomes_.push_back(std::move(outcome));
    }
    // 預けた券の結果を次の collect に載せる。
    void finish(std::uint64_t ticket)
    {
        auto found = held_.find(ticket);
        outcomes_.emplace_back(
            nenenib::application::DocumentReadFinished{ticket, std::move(found->second)});
        held_.erase(found);
    }
    [[nodiscard]] const std::vector<nenenib::application::DocumentReadRequest> &
    requests() const noexcept
    {
        return requests_;
    }
    [[nodiscard]] const std::vector<std::uint64_t> &cancels() const noexcept
    {
        return cancels_;
    }
    [[nodiscard]] std::size_t collects() const noexcept
    {
        return collects_;
    }

  private:
    nenenib::application::FilePort &files_;
    nenenib::application::CodePagePort &code_pages_;
    std::size_t defer_above_ = std::numeric_limits<std::size_t>::max();
    std::vector<nenenib::application::DocumentReadRequest> requests_;
    std::vector<std::uint64_t> cancels_;
    std::vector<nenenib::application::DocumentReadOutcome> outcomes_;
    std::map<std::uint64_t,
             std::expected<nenenib::application::OpenedDocument, nenenib::application::FileFailure>>
        held_;
    std::size_t collects_ = 0;
};
} // namespace nenenib::tests
