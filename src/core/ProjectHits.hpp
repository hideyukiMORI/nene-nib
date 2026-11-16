#pragma once

#include "ProjectChunkHits.hpp"
#include "ProjectQuery.hpp"
#include "ProjectScan.hpp"

#include <cstddef>
#include <string_view>

namespace nenenib::core
{
// 1 ファイルの塊 1 つを照合する純関数（ADR 0167 の決定 3）。worker が塊ごとに呼ぶ。
// - chunk はファイルのバイト列の続き（大きさは問わない・行の境で切らなくてよい）。scan は前の塊の
//   答えの next（最初の塊は既定の ProjectScan{}）。行は `\n` で区切り、行末の CR とファイルの
//   先頭の UTF-8 の BOM は本文に入れない。
// - 塊の終わりで切れた行は next.carry に繰り越し、次の塊の先頭の続きと 1 行にして照合する
//   （最大 1 行）。project_search_line_limit を超える行は照合せずに飛ばす（行の番号は数える）。
// - 正しい UTF-8 でない行（Shift_JIS ほか）は照合しない（当たりの行と桁が文書の本文と揃わない）。
// - 当たりは 1 行に 1 つ（最も左）で、多くて room 件。room に届いたら残りの行は照合しない
//   （next の行の数と繰り越しは正しいまま）。
// 最後の塊の後に project_hits_finished を 1 回呼ぶ。塊に切って順に渡した当たりの列は、全文を
// 1 つの塊で渡した当たりの列と同じ（契約の性質の試験）。
[[nodiscard]] ProjectChunkHits project_hits_in(const ProjectScan &scan, std::string_view chunk,
                                               const ProjectQuery &query, std::size_t room);

// ファイルの終わり: 改行で終わらない最後の行（scan.carry）を照合し、next を空の繰り越しにする。
[[nodiscard]] ProjectChunkHits project_hits_finished(const ProjectScan &scan,
                                                     const ProjectQuery &query, std::size_t room);
} // namespace nenenib::core
