#pragma once

#include <cstdint>

namespace nenenib::core
{
// `:set lsp`・`:set nolsp`・`:set lsp?`（ADR 0160 の決定 7・ADR 0051 の追補の clipboard と同じ
// 要求の形）と `:LspLog`（2026-11-22 の追補・LSP 段 S8）。enable と disable は設定 v4 の `lsp` を
// 書き換えて保存し、言語サーバーを起こす・止めるのは controller。enable は止まった後の手での再開も
// 兼ねる（決定 6）。show は今の値を知らせる問いで、今の値は設定を持つ controller が答える。log は
// 言語サーバーの記録を無題のタブに写して開く要求で、設定は変えない（F1 の一覧の「言語サーバーの
// 記録を開く」と同じ道）。値が増えたら写し先の switch がコンパイルで落ちる（CPP-002）。
enum class ExLspRequest : std::uint8_t
{
    enable,
    disable,
    show,
    log
};
} // namespace nenenib::core
