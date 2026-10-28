#pragma once

#include <cstddef>
#include <string>

namespace nenenib::application
{
// 面に見える 1 行の表示値（ADR 0162 の決定 2・8）。name は節の名前（more の行は空）、depth は
// ルートの子を 0 とした字下げの段、folder・open・hidden は節の値、more は 2,000 件を超えて末尾を
// 捨てたフォルダの後ろに置く「…」の行（core::ExplorerRow の写し）。renderer は写すだけ。
struct ExplorerRowView
{
    std::string name;
    std::size_t depth;
    bool folder;
    bool open;
    bool hidden;
    bool more;
};
} // namespace nenenib::application
