#pragma once

#include <string>

namespace nenenib::application
{
// IME が確定した文字列（GCS_RESULTSTR）。通常モードは InsertText と同じ 1 本で 1 つの undo
// 単位になり、Vim の INSERT では code point ごとの打鍵として入る（ADR 0014 の決定 4）。
struct CommitText
{
    std::string utf8;
};
} // namespace nenenib::application
