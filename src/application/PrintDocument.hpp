#pragma once

#include "PrintFailure.hpp"
#include "PrintSettings.hpp"

#include <expected>

namespace nenenib::application
{
// アクティブな文書を印刷する（ADR 0177 の決定 1・10）。ui が F1 の一覧の「印刷」と Vim の
// `:hardcopy`（controller が operation_request を立てる）の両方で先に印刷のダイアログを出し、
// 選んだ設定か、ダイアログの失敗（no_printer / dialog_failed）をこの 1 意図で送る。取り消しは
// 送らない。本文と字体は controller が状態から写す。使う人の操作なので keeps_tab_walk にも
// begin_intent の表にも書かない。
struct PrintDocument
{
    std::expected<PrintSettings, PrintFailure> settings;
};
} // namespace nenenib::application
