#pragma once

#include "FilePath.hpp"
#include "OpenedDocument.hpp"

namespace nenenib::application
{
// 言語サーバーの編集の先のうち、タブの無いファイル（#2004・ADR 0194 の決定 5）。path は開く道と
// 同じパス、document は開く道と同じ読みの port で検査のときに読んだ本文（当てるときに固定のタブ
// として置く）。公開 aggregate（CPP-003）。
struct UnopenedFile
{
    core::FilePath path;
    OpenedDocument document;
};
} // namespace nenenib::application
