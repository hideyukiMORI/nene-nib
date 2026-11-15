#pragma once

namespace nenenib::application
{
// バージョン情報を知らせる（施主決定 D90・#1677）。F1 の一覧の操作「バージョン情報」が送る。中身は
// 無く、controller が core::app_about_line の 1 行をステータスバーの知らせにする（Vim の `:version`
// は Ex の答えの文で同じ 1 行を出す）。使う人の操作なので keeps_tab_walk にも begin_intent の表にも
// 書かない。
struct ShowVersion
{
};
} // namespace nenenib::application
