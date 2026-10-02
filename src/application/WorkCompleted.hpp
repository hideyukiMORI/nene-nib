#pragma once

namespace nenenib::application
{
// 裏の仕事が読めた分を置いた、という合図を ui が写した意図（ADR 0062 の決定 7）。中身は無く、
// 読めた分は controller が port の collect() で引く。使う人の操作ではないので、歩き・知らせ・
// 変換・入力行・Vim の待ちの状態・記録・undo の単位を 1 つも動かさない。
struct WorkCompleted
{
};
} // namespace nenenib::application
