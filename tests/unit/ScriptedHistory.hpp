#pragma once

#include "FileHistory.hpp"
#include "FileHistoryFailure.hpp"
#include "HistoryPort.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <utility>

namespace nenenib::tests
{
using HistoryReading =
    std::expected<nenenib::application::FileHistory, nenenib::application::FileHistoryFailure>;

// 閉じたファイルの履歴の替え玉（ADR 0060 の決定 8）。仕込んだ読みを返し、書かれた値は次の読みが
// 返す（本物のファイルと同じ往復）。読みと書きの回数を数え、書きの失敗を仕込める。
class ScriptedHistory final : public nenenib::application::HistoryPort
{
  public:
    explicit ScriptedHistory(HistoryReading reading = nenenib::application::FileHistory{})
        : reading_(std::move(reading))
    {
    }

    [[nodiscard]] HistoryReading read() override
    {
        ++reads_;
        return reading_;
    }

    [[nodiscard]] std::expected<void, nenenib::application::FileHistoryFailure>
    write(const nenenib::application::FileHistory &history) override
    {
        ++writes_;
        if (failure_.has_value())
        {
            return std::unexpected(failure_.value());
        }
        written_ = history;
        reading_ = history;
        return {};
    }

    // read が呼ばれた回数（起動と「開く」の道で履歴を読まないことを見る・決定 8）。
    [[nodiscard]] std::size_t reads() const noexcept
    {
        return reads_;
    }
    [[nodiscard]] std::size_t writes() const noexcept
    {
        return writes_;
    }
    [[nodiscard]] const std::optional<nenenib::application::FileHistory> &written() const noexcept
    {
        return written_;
    }
    void fail(std::optional<nenenib::application::FileHistoryFailure> failure)
    {
        failure_ = failure;
    }

  private:
    HistoryReading reading_;
    std::size_t reads_ = 0;
    std::size_t writes_ = 0;
    std::optional<nenenib::application::FileHistory> written_;
    std::optional<nenenib::application::FileHistoryFailure> failure_;
};
} // namespace nenenib::tests
