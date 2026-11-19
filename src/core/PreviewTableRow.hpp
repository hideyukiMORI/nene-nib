#pragma once

#include "Offset.hpp"
#include "PreviewTableAlign.hpp"

#include <cstdint>
#include <vector>

namespace nenenib::core
{
// 表の 1 行（要素 table_row が持つ・#1848・D103）。row は表の中の行の番号（見出しの行が 0）で、
// 表の最初の行の要素は自分の番号から row を引いた位置。header は見出しの行か。cells は要素の text
// の中の升の始まり（升の間は Tab 1 つ）、aligns は列の揃え（cells と同じ数）。比較は非メンバー
// （CPP-003）。digest は表の全部の行の升の文字・装飾・見出しかの要約（同じ表の行は同じ値・
// preview_table_digest・列の幅の覚えの鍵）で、比べない（文字と装飾から決まる写し）。table は
// 文書の中の表の通し番号（0 から・同じ表の行は同じ値・横の送りの量を引く鍵・#1883・D107）。
struct PreviewTableRow
{
    std::uint32_t row;
    bool header;
    std::vector<Offset> cells;
    std::vector<PreviewTableAlign> aligns;
    std::uint64_t digest;
    std::uint32_t table = 0;
};

[[nodiscard]] inline bool operator==(const PreviewTableRow &left, const PreviewTableRow &right)
{
    return left.row == right.row && left.header == right.header && left.cells == right.cells &&
           left.aligns == right.aligns && left.table == right.table;
}
} // namespace nenenib::core
