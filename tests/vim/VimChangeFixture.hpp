#pragma once

namespace nenenib::tests
{
// fixture の `change`（ADR 0181・#1440）。oracle が鍵の前後で測った Vim の 3 つの印で、求めた
// fixture だけが持つ。modified は鍵の後の &modified（鍵の前に nomodified）、changed は
// b:changedtick が進んだか（Vim が changed_* を呼んだ＝C）、undo は undotree().seq_cur が進んだか
// （u_save が undo の単位を作った＝U・1 本の :normal! の中は 1 単位に畳まれるので真偽だけ）。
struct VimChangeFixture
{
    bool modified;
    bool changed;
    bool undo;
};
} // namespace nenenib::tests
