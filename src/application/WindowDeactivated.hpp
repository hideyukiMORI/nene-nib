#pragma once

namespace nenenib::application
{
// 窓が非アクティブになった知らせ（ADR 0190 の決定 6）。ui が WM_KILLFOCUS で送り、controller は
// hover の札を消す。使う人の操作ではない（窓の寸法の知らせと同じ側・not_user_operation の 1 本に
// 載る）ので、歩き・知らせ・変換・入力行・Vim の待ちの状態・記録・undo の単位を 1 つも動かさない。
// 札が出ていなければ描き直さない。
struct WindowDeactivated
{
};
} // namespace nenenib::application
