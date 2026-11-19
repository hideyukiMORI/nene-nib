#pragma once

#include <string>

namespace nenenib::application
{
// 設定の面の探す欄の文字を置き換える（追補 2・名前・説明・`:set` の名前で絞る）。ui は frame の
// 探す文字に打った文字を足した全体を送る。焦点の行が見えなくなれば最初の見えている行へ移る。
// 使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct FilterSettings
{
    std::string query;
};
} // namespace nenenib::application
