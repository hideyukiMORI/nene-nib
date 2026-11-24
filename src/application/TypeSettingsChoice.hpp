#pragma once

#include <string>

namespace nenenib::application
{
// 開いている選ぶ欄の一覧の頭文字の跳び（#1926）。typed は続けて打った字の全体（ui が
// 表示値の打った字に足した・Backspace なら 1 字消した文字）。空でなければ、表示の名前が
// typed で始まる最初の選択肢（前の方から・ASCII の大小は区別しない）へ「選ぼうとして
// いる位置」を置く。当たりが無ければ位置はそのまま。打った字を忘れるのは一覧を閉じた
// ときと 1 秒の合図 SettingsChoiceTimeout。一覧が開いていなければ何もしない。使う人の
// 操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct TypeSettingsChoice
{
    std::string typed;
};
} // namespace nenenib::application
