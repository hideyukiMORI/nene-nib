#pragma once

#include "FileHistory.hpp"
#include "FileHistoryFailure.hpp"

#include <expected>

namespace nenenib::application
{
// 閉じたファイルの履歴を読み書きする口（ADR 0060 の決定 8）。保存形式と場所は adapters に閉じる。
// read は、ファイルが無ければ空の履歴を返す（「無い」と「空」を分けない）。
class HistoryPort
{
  public:
    HistoryPort() = default;
    virtual ~HistoryPort() = default;
    HistoryPort(const HistoryPort &) = delete;
    HistoryPort(HistoryPort &&) = delete;
    HistoryPort &operator=(const HistoryPort &) = delete;
    HistoryPort &operator=(HistoryPort &&) = delete;

    [[nodiscard]] virtual std::expected<FileHistory, FileHistoryFailure> read() = 0;
    [[nodiscard]] virtual std::expected<void, FileHistoryFailure>
    write(const FileHistory &history) = 0;
};
} // namespace nenenib::application
