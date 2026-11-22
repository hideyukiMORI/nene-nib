#pragma once

#include "FilePath.hpp"
#include "LspLanding.hpp"
#include "PreviewAnchor.hpp"

namespace nenenib::application
{
// Ctrl+P の参照の候補 1 つが指す所（#1947・ADR 0191 の決定 4）。面を開くときに controller が控え、
// 候補の command はこの列の位置。file はファイル、at は答えの位置（答えた席の単位のまま・跳ぶ
// ときに開いた本文で写す）、anchor はプレビュー欄の窓の中心と印（開いていないファイルは行だけ）。
// 公開 aggregate（CPP-003）。
struct PaletteReference
{
    core::FilePath file;
    LspLanding at;
    PreviewAnchor anchor;
};
} // namespace nenenib::application
