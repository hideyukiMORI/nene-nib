#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace nenenib::tests
{
// adapters の試験がファイルを書く置き場（Issue #1103）。GetTempPathW の下の
// `nib-tests-<10 進の pid>-<name>` を作り、cwd をそこへ移して経路を返す。試験の相対の経路は全部
// この中に解決され、試験を走らせた cwd（作業木の根・ビルドディレクトリ）には何も書かない。
// 作る前に、同じ name の前の走りの残骸（pid のプロセスが生きていないものと自分の pid のもの）を
// 消す。途中で落ちて消し損ねても、次の走りがここで消す。作れなければ無し。
[[nodiscard]] std::optional<std::wstring> temporary_test_directory(std::wstring_view name);
// cwd を一時フォルダの親へ戻してから、temporary_test_directory が返した置き場を中身ごと消す。
void remove_temporary_test_directory(const std::wstring &directory);
} // namespace nenenib::tests
