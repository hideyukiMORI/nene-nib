#pragma once

#include "Diagnostic.hpp"

#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace nenenib::application
{
// 文書の値の診断（ADR 0161 の決定 2・#1939 のレビュー R3 の推奨 2）。置いた後は変えない列を指す
// 参照で、状態の写し（打鍵ごとの with_*）は参照 1 つだけを写す。空は無し（nullptr）。作るのも
// 読むのも UI の糸の上だけ（言語サーバーの知らせを受けるのは WorkCompleted の collect の後・
// worker へは渡さない）。
using SharedDiagnostics = std::shared_ptr<const std::vector<core::Diagnostic>>;

[[nodiscard]] inline std::span<const core::Diagnostic>
diagnostics_of(const SharedDiagnostics &diagnostics) noexcept
{
    return diagnostics == nullptr ? std::span<const core::Diagnostic>{}
                                  : std::span<const core::Diagnostic>{*diagnostics};
}

// 列を参照にする（空なら無し）。
[[nodiscard]] inline SharedDiagnostics shared_diagnostics(std::vector<core::Diagnostic> diagnostics)
{
    return diagnostics.empty()
               ? nullptr
               : std::make_shared<const std::vector<core::Diagnostic>>(std::move(diagnostics));
}
} // namespace nenenib::application
