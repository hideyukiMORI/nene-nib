#pragma once

#include <string>

namespace nenenib::core
{
// フォルダを読んだ答えの 1 項目（ADR 0162 の決定 1・5）。application が FolderPort の答えから名前・
// フォルダか・隠し属性かを写して ExplorerTree::loaded に渡す。core は OS に触れないので、経路と
// 属性の読みは adapters の仕事（ARC-003）。
struct ExplorerEntry
{
    std::string name;
    bool folder;
    bool hidden;
};
} // namespace nenenib::core
