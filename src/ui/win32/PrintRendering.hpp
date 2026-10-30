#pragma once

#include "PrintFailure.hpp"
#include "PrintJob.hpp"
#include "PrintSetup.hpp"
#include "PrintSurface.hpp"

#include <DocumentTarget.h>
#include <expected>

namespace nenenib::ui::win32
{
// 紙面を組んで print control に流す 1 本（ADR 0177 の決定 3・8）。surface.device の
// CreatePrintControl に target を渡し、setup.ranges のページ（空なら全ページ）ごとに command list
// へ draw_print_page で描いて AddPage し、最後に Close する。製品は OS のスプールの target、
// tests/ui はメモリの XPS へ書く target を渡す（実機のプリンタを使わない）。途中で失敗したら
// target を Cancel して rendering_failed。
[[nodiscard]] std::expected<void, application::PrintFailure>
render_print_job(const PrintSurface &surface, IPrintDocumentPackageTarget &target,
                 const application::PrintJob &job, const PrintSetup &setup);
} // namespace nenenib::ui::win32
