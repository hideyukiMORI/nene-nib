#pragma once

#include "AutosaveId.hpp"

#include <cstdint>

namespace nenenib::application
{
// 補完を出したときの文脈（ADR 0190 の決定 1）。session は要求を出した補完の面の通し番号（面は
// 段 B が作り、呼び出し元が渡す）、document は文書のタブの退避番号。答えを使うのは、同じ文書が
// アクティブなときだけ（面がまだ開いているかは session で使う側が比べる）。公開 aggregate
// （CPP-003）。
struct LspCompletionContext
{
    std::uint64_t session;
    AutosaveId document;
};
} // namespace nenenib::application
