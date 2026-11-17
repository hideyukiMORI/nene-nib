#pragma once

namespace nenenib::application
{
// メニューを閉じる（Esc・メニューの外のクリック・窓が焦点を失ったとき・ADR 0078 の追補 #1051）。
// ほかの使う人の操作の意図も、写す前に入口の 1 か所（close_context_menu_before）が閉じる。使う人の
// 操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct CloseContextMenu
{
};
} // namespace nenenib::application
