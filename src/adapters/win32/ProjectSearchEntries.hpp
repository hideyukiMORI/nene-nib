#pragma once

#include <windows.h>

namespace nenenib::adapters::win32
{
// 全文検索の歩きが列挙の 1 項目を歩くか・読むかの判定（ADR 0167 の決定 2 と G3 の追補 2）。属性と
// リパースのタグ（FindFirstFileExW の WIN32_FIND_DATAW::dwReserved0・属性に
// FILE_ATTRIBUTE_REPARSE_POINT があるときだけ意味を持つ）だけを読む純関数で、OS に問わない。

// フォルダを歩くか。隠し属性のフォルダと、ジャンクション（IO_REPARSE_TAG_MOUNT_POINT）・
// シンボリックリンク（IO_REPARSE_TAG_SYMLINK）のフォルダ（循環を避ける）は歩かない。ほかのタグの
// リパースポイント（OneDrive などクラウドの置き場のフォルダ）は歩く。名前の表は呼び出し元が見る。
[[nodiscard]] bool project_folder_walked(DWORD attributes, DWORD reparse_tag) noexcept;

// ファイルを読むか。中身がまだ手元に無いファイル（FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS・
// FILE_ATTRIBUTE_OFFLINE）は読まない（探すだけで取り寄せを起こさない・数えない）。大きさの上限は
// 呼び出し元が見る。
[[nodiscard]] bool project_file_read(DWORD attributes) noexcept;
} // namespace nenenib::adapters::win32
