#pragma once

#include "PrintPort.hpp"

namespace nenenib::ui::win32
{
// 印刷の口の実装（ADR 0177 の決定 3）。DirectWrite の書式と BodyTextPainter を使うので renderer の
// 隣に置く（TextLayoutPort と同じく ui/win32 が application の port を実装する）。
// 手順: PrintDlgExW（choose_printer）→ WARP の Direct2D device → IPrintDocumentPackageTargetFactory
// の job の受け口 → render_print_job（ページごとに command list へ描いて ID2D1PrintControl へ）。
// ダイアログの親はいま操作している窓（GetActiveWindow）。状態を持たない。
class Win32Printer final : public application::PrintPort
{
  public:
    [[nodiscard]] std::expected<application::PrintOutcome, application::PrintFailure>
    print(application::PrintJob job) override;
};
} // namespace nenenib::ui::win32
