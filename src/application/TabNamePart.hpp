#pragma once

#include <cstdint>

namespace nenenib::application
{
// タブのファイルの名前のどこを写すか（ADR 0078 の追補 #1790）。path は完全なパス、file_name は
// 最後の部分（拡張子を含む）。
enum class TabNamePart : std::uint8_t
{
    path,
    file_name
};
} // namespace nenenib::application
