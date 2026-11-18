#pragma once

#include "CompletionSource.hpp"
#include "LspRange.hpp"

#include <optional>
#include <string>

namespace nenenib::core
{
// 補完の面に並べる候補 1 つ（ADR 0190 の決定 5・ADR 0169 の決定 5）。label は面に出す名前、detail
// は右に添える補足（無ければ空）、filter は接頭辞で絞る鍵、insert は確定で本文に入れる平文、range は
// LSP の `textEdit` の範囲（無ければ接頭辞を置き換える）、source は出どころ。本文の語は label と
// filter と insert が同じ語。どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct CompletionEntry
{
    std::string label;
    std::string detail;
    std::string filter;
    std::string insert;
    std::optional<LspRange> range;
    CompletionSource source;
};
} // namespace nenenib::core
