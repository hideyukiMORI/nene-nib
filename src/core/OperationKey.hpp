#pragma once

#include <cstdint>

namespace nenenib::core
{
// 操作の割り当ての表が使う鍵だけの閉じた一覧（ADR 0078 の決定 2）。主キーとテンキーの + - 0 は
// ui の写しが同じ値にする。鍵を足すと表示名の switch がコンパイルで落ちる（CPP-002）。
enum class OperationKey : std::uint8_t
{
    o,
    s,
    t,
    w,
    p,
    d,
    z,
    y,
    f1,
    f4,
    f6,
    tab,
    plus,
    minus,
    zero,
    space,
    // `\`（JIS の ¥ と ろ の 2 つの物理キーを ui が同じ値に写す・ADR 0148 の決定 6）。
    backslash,
    m,
    // Ctrl+Shift+V の文字だけの貼り付け（ADR 0157 の決定 2）。
    // Shift なしの Ctrl+V は表に行を置かない（ui の今の貼り付けの道のまま）。
    v,
    // Ctrl+Shift+E のエクスプローラー（ADR 0162 の決定 7）。Shift なしの Ctrl+E
    // は表に行を置かない。
    e,
    // 修飾なしの F5 の日時の挿入（ADR 0175 の決定 3）。
    f5,
    // 通常モードの検索・置換・行へ移動の Ctrl+F・Ctrl+H・Ctrl+G と F3 / Shift+F3（ADR 0174 の
    // 決定 1）。Vim の `gg` の `g` とは関係が無い。
    f,
    h,
    g,
    f3
};
} // namespace nenenib::core
