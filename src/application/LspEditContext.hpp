#pragma once

#include "LanguageServerSlot.hpp"
#include "LspRequestContext.hpp"
#include "OpenedRevision.hpp"

#include <vector>

namespace nenenib::application
{
// 答えが `WorkspaceEdit` になる要求の文脈（#2004・ADR 0194 の決定 2・4）。
// request は頼んだ文書・改訂番号・キャレット（照合 (a) は文書と改訂番号
// だけを比べる）、opened は帯の読み込み済みのタブごとの写し（照合 (b)(c)・
// #2011 のレビュー M1・R6 の N1）、slot は頼んだ（答える）席（当てた文書を
// 開く先・R6 の P1）。hover などの文脈（LspRequestContext）に写しを
// 足さないのは、frame ごとに比べる道（shown_hover）にタブの列を作る仕事を
// 足さないため。公開 aggregate（CPP-003）。
struct LspEditContext
{
    LspRequestContext request;
    std::vector<OpenedRevision> opened;
    LanguageServerSlot slot;
};
} // namespace nenenib::application
