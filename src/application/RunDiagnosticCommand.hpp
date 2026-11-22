#pragma once

#include "DiagnosticCommand.hpp"

namespace nenenib::application
{
// 操作「次の診断へ」（F8）「前の診断へ」（Shift+F8）「診断を表示」「診断の一覧」が送る（#1939）。
// Vim の `]d` `[d` `gl` は engine の効果（core::VimDiagnosticCommand）から同じ 1 本
// （EditorController::run_diagnostic_command）へ入る。文書の値の診断だけを読み、サーバーへは何も
// 頼まない。使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct RunDiagnosticCommand
{
    core::DiagnosticCommand command;
};
} // namespace nenenib::application
