#pragma once

#include "PrintFailure.hpp"
#include "PrintJob.hpp"
#include "PrintOutcome.hpp"

#include <expected>

namespace nenenib::application
{
// 印刷の唯一の口（ADR 0177 の決定 2）。プリンタを選ぶダイアログ・文字組み・スプールへの受け渡しは
// 実装（src/ui/win32 の Win32Printer・決定 3）だけが持つ。同期で、ダイアログはモーダル。controller
// が呼ぶのは意図 PrintDocument の 1 か所だけ（起動・開く・切り替え・保存・打鍵では呼ばない）。
class PrintPort
{
  public:
    PrintPort() = default;
    virtual ~PrintPort() = default;
    PrintPort(const PrintPort &) = delete;
    PrintPort(PrintPort &&) = delete;
    PrintPort &operator=(const PrintPort &) = delete;
    PrintPort &operator=(PrintPort &&) = delete;

    [[nodiscard]] virtual std::expected<PrintOutcome, PrintFailure> print(PrintJob job) = 0;
};
} // namespace nenenib::application
