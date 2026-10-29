#pragma once

#include "ExplorerTree.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::application
{
// エクスプローラーの状態（ADR 0162 の決定 3）。窓に 1 つで、タブの切り替えでは変わらない。
// tree は設定 root から作った木、selected は見えている行（tree.visible_rows()）の位置、focused は
// 面に焦点があるか、first_visible は面の見えている行の先頭。出ているかは設定 explorer が決め、
// ここには持たない。開いているフォルダと選択は保存しない。どの欄も単独で妥当な値なので公開
// aggregate（CPP-003）。
struct ExplorerState
{
    core::ExplorerTree tree;
    std::optional<std::size_t> selected;
    bool focused = false;
    std::size_t first_visible = 0;
};
} // namespace nenenib::application
