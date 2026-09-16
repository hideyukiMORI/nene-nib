#pragma once

#include <string>

namespace nenenib::core
{
// INSERT で打たれた文字（UTF-8）。改行は VimNewLine が運ぶので、ここには入らない。
struct VimInsertString
{
    std::string utf8;
};
} // namespace nenenib::core
