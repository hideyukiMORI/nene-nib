#pragma once

#include "EditHistory.hpp"
#include "SubstituteConfirmation.hpp"
#include "TextBuffer.hpp"

namespace nenenib::application
{
// `:s///c` の問いの入力行（ADR 0168 の決定 6・段 2a）。CommandInput の 1 値で、開いている間は
// Vim の鍵も通常モードの鍵も本文へ通らず、文字は答え（y n a q l）としてだけ読む。confirmation は
// engine の問いの値、history と text は問いを開いたときの履歴と本文で、問いを終えるときに置いた分を
// 1 つの undo の単位へ畳む（途中で止めても 1 単位）。値は単独で妥当なので公開
// aggregate（CPP-003）。
struct SubstituteQuestion
{
    core::SubstituteConfirmation confirmation;
    core::EditHistory history;
    core::TextBuffer text;
};
} // namespace nenenib::application
