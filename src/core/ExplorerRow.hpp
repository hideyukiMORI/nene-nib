#pragma once

#include <cstddef>

namespace nenenib::core
{
// 面に見える 1 行（ADR 0162 の決定 1）。node は木の節の番号、depth はルートの子を 0 とした字下げの
// 段。more は 2,000 件を超えて末尾を捨てたフォルダの子の後ろに置く「…」の行で、node はそのフォルダ
// （ルートなら 0）を指し、depth は子と同じ段。folder・open・hidden は more の行では偽。
struct ExplorerRow
{
    std::size_t node;
    std::size_t depth;
    bool folder;
    bool open;
    bool hidden;
    bool more;
};
} // namespace nenenib::core
