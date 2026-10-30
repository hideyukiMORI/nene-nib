#pragma once

#include "PrintPort.hpp"

namespace nenenib::ui::win32
{
// 印刷の口の実装（ADR 0177 の決定 3）。DirectWrite の書式と BodyTextPainter を使うので renderer の
// 隣に置く（TextLayoutPort と同じく ui/win32 が application の port を実装する）。
// いまは口だけで、PrintDlgExW・XPS の print control・ページの描画は #1047 の N6b
// が入れる。それまでは ダイアログを出さずに no_printer を返す（OS の印刷の API をまだ呼ばない）。
class Win32Printer final : public application::PrintPort
{
  public:
    [[nodiscard]] std::expected<application::PrintOutcome, application::PrintFailure>
    print(application::PrintJob job) override;
};
} // namespace nenenib::ui::win32
