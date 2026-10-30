#pragma once

namespace nenenib::application
{
// エクスプローラーへの登録を外す（ADR 0178 の決定 3）。登録が無ければ書かずに知らせる。中身は無い。
// 使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct UnregisterShell
{
};
} // namespace nenenib::application
