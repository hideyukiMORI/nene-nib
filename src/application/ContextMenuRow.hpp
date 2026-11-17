#pragma once

#include "EditorOperation.hpp"

#include <string>

namespace nenenib::application
{
// メニューの 1 行の表示値（ADR 0078 の追補 #1051）。name は操作の表の名前を画面の言語で、key は
// core::context_menu_key の鍵の表示。separated は行の上に区切り線があること。
struct ContextMenuRow
{
    core::EditorOperation operation;
    std::string name;
    std::string key;
    bool enabled;
    bool separated;
};
} // namespace nenenib::application
