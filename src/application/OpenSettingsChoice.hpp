#pragma once

namespace nenenib::application
{
// 設定の面の焦点の行の選ぶ欄（core::SettingsControl::choice）の一覧を開く（#1926・
// 採用案 2026-11-24）。値の箱のクリック・焦点の行の Enter / Space（探す欄が空のとき）・
// Alt+↓・F4。選ぼうとしている位置は今の値（列に無ければ先頭）で、見える範囲の中ほどに
// 来るように送る。焦点の行が選ぶ欄でなければ・面が閉じていれば・もう開いていれば何も
// しない。使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct OpenSettingsChoice
{
};
} // namespace nenenib::application
