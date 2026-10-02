#pragma once

#include "FilePath.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nenenib::application
{
// Ctrl+P の面が同じフォルダを頼んだ控え（ADR 0062 の決定 14・15・17）。ticket は面を開くたびに
// 1 つ進める券で、届いた batch がいま開いている面の分かを見分ける。listed は面を開いたときに
// 一覧にもう出ていた、頼んだフォルダにあるファイル（開いているタブと履歴の候補）で、届いた
// ファイルはこれとだけ比べる。received は今の券で届いたファイルの数（除く前）で、打ち切りの知らせの
// 数になる。値だけを運ぶ公開 aggregate（CPP-003）。
struct PaletteFolder
{
    std::uint64_t ticket = 0;
    std::vector<core::FilePath> listed;
    std::size_t received = 0;
};
} // namespace nenenib::application
