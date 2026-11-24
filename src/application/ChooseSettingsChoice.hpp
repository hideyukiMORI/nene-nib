#pragma once

namespace nenenib::application
{
// 開いている選ぶ欄の一覧の「選ぼうとしている位置」の値に決めて一覧を閉じる（#1926）。Enter・Space・
// Tab と、行を押して同じ行で離したとき。決めた値は ChangeSetting と同じ道（即時に反映・閉じると
// 保存・「元に戻す」が効く・ADR 0170 の決定 5・6）。焦点は同じ行のまま。一覧が開いていなければ何も
// しない。使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct ChooseSettingsChoice
{
};
} // namespace nenenib::application
