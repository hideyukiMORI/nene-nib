#pragma once

#include <string>
#include <vector>

namespace nenenib::adapters::win32
{
// 言語サーバーを探す置き場（LSP 段 S4・#1945）。folders は実行ファイルと node.exe を探すフォルダ
// （探す順・絶対パスだけ）、npm_roots は npm のグローバルの置き場（`<root>\node_modules\<包み>`
// を見る・絶対パスだけ）。製品は system_search_places が環境変数から作り、試験は作った一時の
// フォルダを渡す（本物の PATH に依らない）。
struct ServerSearchPlaces
{
    std::vector<std::wstring> folders;
    std::vector<std::wstring> npm_roots;
};
} // namespace nenenib::adapters::win32
