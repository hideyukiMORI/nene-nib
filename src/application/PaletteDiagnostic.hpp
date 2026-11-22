#pragma once

#include "PreviewAnchor.hpp"
#include "TextPosition.hpp"

#include <cstddef>

namespace nenenib::application
{
// Ctrl+P の診断の候補 1 つが指す所（#1939）。面を開くときに controller が控え、候補の command は
// この列の位置。tab は帯の位置（面を開いている間は帯が変わらない・ADR 0062 の決定 15）、at は
// その文書の本文の行と桁、anchor はプレビュー欄の窓の中心と印（ADR 0189）。公開 aggregate
// （CPP-003）。
struct PaletteDiagnostic
{
    std::size_t tab;
    core::TextPosition at;
    PreviewAnchor anchor;
};
} // namespace nenenib::application
