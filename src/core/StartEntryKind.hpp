#pragma once

#include <cstdint>

namespace nenenib::core
{
// 起動画面の選べる行の種類（ADR 0172 の決定 2）。最近使ったファイル（履歴）・ブックマーク・
// 「はじめる」の 3 つ。鍵の案内の節は選べないので値を持たない。値が増えたら実行の switch が落ちる
// （CPP-002）。
enum class StartEntryKind : std::uint8_t
{
    recent,
    bookmark,
    new_file,
    open_file,
    open_folder
};
} // namespace nenenib::core
