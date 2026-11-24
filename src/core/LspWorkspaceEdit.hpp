#pragma once

#include "LspFileEdits.hpp"

#include <vector>

namespace nenenib::core
{
// 読んだ `WorkspaceEdit`（ADR 0194 の決定 4）。files は答えの順のファイルごとの編集（編集の無い
// ファイルは持たない）。空なら変える所が無い（null の答えも空）。公開 aggregate（CPP-003）。
struct LspWorkspaceEdit
{
    std::vector<LspFileEdits> files;
};
} // namespace nenenib::core
