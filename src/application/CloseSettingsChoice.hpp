#pragma once

namespace nenenib::application
{
// 開いている選ぶ欄の一覧を決めずに閉じる（#1926）。Esc・Alt+↑・F4・一覧の外の押し・右クリック・窓の
// 大きさが変わったとき。値は変えず、面は閉じない（Esc は内側の一覧から先に閉じる）。一覧が開いて
// いなければ何もしない。使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct CloseSettingsChoice
{
};
} // namespace nenenib::application
