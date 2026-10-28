#pragma once

#include <cstdint>

namespace nenenib::application
{
// フォルダを読む要求が何を返してほしいか（ADR 0162 の決定 5）。files は直下のファイルのフルパス
// だけ（Ctrl+P の同じフォルダ・ADR 0062）、entries は直下の項目（フォルダも隠しも・`.` `..` を
// 除く）を FolderEntry で。名前は adapters の FolderListing（列挙の仕事）と分けて Kind を付ける。
enum class FolderListingKind : std::uint8_t
{
    files,
    entries
};
} // namespace nenenib::application
