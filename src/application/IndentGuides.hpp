#pragma once

#include <cstddef>
#include <optional>

namespace nenenib::application
{
// 見えている 1 行のインデントガイド（ADR 0193 の決定 1・9・#1975）。cells は行の字下げの仮想桁
// （空の行は上下の続きの値）、step は段の幅の桁、levels は引く段の数（cells / step）。段 i の線は
// 仮想桁 i × step に引く。active は強める段（操作中の枠のキャレットの区間だけ・値なしは強めない）。
// renderer は最初の画面行の高さにだけ引く。公開 aggregate（CPP-003）。
struct IndentGuides
{
    std::size_t cells;
    std::size_t step;
    std::size_t levels;
    std::optional<std::size_t> active = std::nullopt;
};
} // namespace nenenib::application
