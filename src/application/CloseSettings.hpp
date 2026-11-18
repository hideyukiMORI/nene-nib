#pragma once

namespace nenenib::application
{
// 設定の面を閉じる（Esc・閉じるボタン・追補 2）。写しと違えば persist_settings と同じ port の
// 書き込みを 1 回だけ通す。ほかの使う人の操作の意図も、入口の 1 か所（close_settings_before）が
// 同じ道で閉じる。使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct CloseSettings
{
};
} // namespace nenenib::application
