#pragma once

#include "AutosaveId.hpp"
#include "Offset.hpp"
#include "VimBlockEdit.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::application
{
// 整形が積んだ undo の単位と、その単位を組んだ編集の列（#1997 の 2 回目のレビュー P1・ADR 0191 の
// 決定 7）。履歴の単位は畳んだ 1 区間しか持たないので、`u` `Ctrl-r`（通常モードの Ctrl+Z / Ctrl+Y）
// でマークともう 1 つの枠を編集ごとに直すために controller が脇に残す。ふつうの単位は持たない
// （履歴の 1 単位の大きさと打鍵の道は変えない）。document は退避番号、index は履歴の中の位置。
// at・removed_length・restore は積んだ単位の編集の始まり・消した長さ・戻り先で、単位の本文の写しは
// 持たない（3 回目のレビュー P3）。引くときは、履歴の単位の removed に pieces を当てると inserted
// に なるかで照合する（同じ位置に別の単位が積み直されたら合わないので使わない）。pieces
// は編集の前の 本文の上の重ならない区間を始まりの順に並べた列（newText は文書の改行の形）。公開
// aggregate （CPP-003）。
struct FormattedUnit
{
    AutosaveId document;
    std::size_t index;
    core::Offset at;
    std::size_t removed_length;
    core::Offset restore;
    std::vector<core::VimBlockEdit> pieces;
};
} // namespace nenenib::application
