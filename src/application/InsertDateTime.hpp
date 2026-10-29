#pragma once

namespace nenenib::application
{
// F5 の日時の挿入（ADR 0175 の決定 3）。時刻は controller が ClockPort から読むので中身は無い。
// 使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct InsertDateTime
{
};
} // namespace nenenib::application
