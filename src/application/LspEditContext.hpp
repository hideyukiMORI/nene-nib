#pragma once

#include "LspRequestContext.hpp"
#include "OpenedRevision.hpp"

#include <vector>

namespace nenenib::application
{
// 答えが `WorkspaceEdit` になる要求の文脈（#2004・ADR 0194 の決定 2・4）。request は頼んだ文書・
// 改訂番号・キャレット（照合 (a) は文書と改訂番号だけを比べる）、opened はその席が `didOpen` して
// いた文書ごとの改訂番号の写し（照合 (b)(c)）。hover
// などの文脈（LspRequestContext）に写しを足さない のは、frame
// ごとに比べる道（shown_hover）に席の文書の列を作る仕事を足さないため。公開 aggregate （CPP-003）。
struct LspEditContext
{
    LspRequestContext request;
    std::vector<OpenedRevision> opened;
};
} // namespace nenenib::application
