#pragma once

namespace nenenib::core
{
// タブのメニューを開いた時点の、そのタブの様子（ADR 0078 の追補 #1790）。項目が使えるかはこの 4 つ
// だけで決まる。Vim のモードでも同じ（engine に鍵を流さない）。
struct TabMenuSituation
{
    // パスがある（無題でない）。
    bool named;
    // 中身を読んである（まだ読んでいないタブ UnloadedDocument でない）。
    bool loaded;
    // ほかのタブがある。
    bool others;
    // 帯でこのタブより右にタブがある。
    bool right;
};
} // namespace nenenib::core
