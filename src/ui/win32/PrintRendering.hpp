#pragma once

#include "PrintComposition.hpp"
#include "PrintFailure.hpp"
#include "PrintRange.hpp"
#include "PrintSurface.hpp"

#include <DocumentTarget.h>
#include <expected>
#include <span>

namespace nenenib::ui::win32
{
// 組んだ紙面を print control に流す 1 本（ADR 0177 の決定 3・8）。surface.device の
// CreatePrintControl に target を渡し、ranges のページ（空なら全ページ）ごとに command list へ
// draw_print_page で描いて AddPage し、最後に Close する。製品は OS のスプールの target、
// tests/ui はメモリの XPS へ書く target を渡す（実機のプリンタを使わない）。target を受けた後の
// 失敗（print control が作れない・描けない・Close できない）はすべてここで target を Cancel して
// rendering_failed。紙面（compose）と device は呼び手が target を作る前に作る。
[[nodiscard]] std::expected<void, application::PrintFailure>
render_print_job(const PrintSurface &surface, IPrintDocumentPackageTarget &target,
                 PrintComposition &composition, std::span<const application::PrintRange> ranges);
} // namespace nenenib::ui::win32
