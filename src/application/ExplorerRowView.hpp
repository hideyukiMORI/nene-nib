#pragma once

#include "SyntaxRole.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace nenenib::application
{
// 面に見える 1 行の表示値（ADR 0162 の決定 2・8）。name は節の名前（more の行は空）、depth は
// ルートの子を 0 とした字下げの段、folder・open・hidden は節の値、more は 2,000 件を超えて末尾を
// 捨てたフォルダの後ろに置く「…」の行（core::ExplorerRow の写し）。renderer は写すだけ。
// 採用案 B（#1390・D81）の 3 つ: mark はファイルの印の色の役割（core::explorer_mark_role・値なしは
// muted・フォルダと「…」の行は値なし）、active はアクティブな文書のファイル（名前を semibold）、
// unsaved は開いているタブのどれかが未保存のファイル（行の右端の accent の丸）。
struct ExplorerRowView
{
    std::string name;
    std::size_t depth;
    bool folder;
    bool open;
    bool hidden;
    bool more;
    std::optional<core::SyntaxRole> mark = std::nullopt;
    bool active = false;
    bool unsaved = false;
};
} // namespace nenenib::application
