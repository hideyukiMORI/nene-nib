#pragma once

#include "AutosaveId.hpp"
#include "Edit.hpp"
#include "VimBlockEdit.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::application
{
// 整形が積んだ undo の単位と、その単位を組んだ編集の列（#1997 の 2 回目のレビュー P1・ADR 0191 の
// 決定 7）。履歴の単位は畳んだ 1 区間しか持たないので、`u` `Ctrl-r`（通常モードの Ctrl+Z / Ctrl+Y）
// でマークともう 1 つの枠を編集ごとに直すために controller が脇に残す。ふつうの単位は持たない
// （履歴の 1 単位の大きさと打鍵の道は変えない）。document は退避番号、index は履歴の中の位置。
// edit は積んだ単位の編集そのもの（同じ位置に別の単位が積み直されたら一致しないので使わない）。
// pieces は編集の前の本文の上の重ならない区間を始まりの順に並べた列（newText は文書の改行の形）。
// 公開 aggregate（CPP-003）。
struct FormattedUnit
{
    AutosaveId document;
    std::size_t index;
    core::Edit edit;
    std::vector<core::VimBlockEdit> pieces;
};
} // namespace nenenib::application
