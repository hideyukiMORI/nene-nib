#pragma once

namespace nenenib::application
{
// 帯の上のホイール（ADR 0056 の決定 3）。刻みの数で、手前（負）で右のタブが見える向き・奥（正）で
// 左へ戻る向きに送る。あふれていなければ何も動かない。
struct ScrollTabs
{
    int notches;
};
} // namespace nenenib::application
