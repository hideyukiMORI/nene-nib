#pragma once

#include <functional>

namespace nenenib::adapters::win32
{
// 最後の砦（ADR 0137 の追補 #1625）。捕まえていない C++ の例外と、noexcept を抜けた例外で呼ばれる
// std::terminate の手続きを 1 つ置く。std::set_terminate は呼んだスレッドだけに効く（UCRT は
// スレッドごとの欄に持つ）ので、UI スレッドはこの型を作ったときに、ワーカーは
// arm_last_resort_thread を最初の仕事の頭で呼んで、同じ手続きを置く。
// 手続きは、作ったスレッド（UI）で呼ばれたときだけ backup を 1 回呼び（未保存の退避・投げても
// 受ける）、どのスレッドでも、置いたときに set_terminate が返した前の手続き（UCRT の既定は
// abort）へ渡して終える。箱は出さず、窓にも触れない。使う人への知らせは次の起動の「前回の未保存を戻しますか」が
// 担い、終わり方は既定のまま（製品の Release は __fastfail で WER の記録が残る）。2 度目に入ったら
// （退避の中でまた例外・別のスレッドが同時に）退避せずに前の手続きへ渡す。SEH（アクセス違反・
// stack の溢れ）は受けない。壊すと UI スレッドの印と backup を外し、UI スレッドの手続きを前のものへ
// 戻す（backup は持ち主の controller より先に消える）。合成ルートが起動のときに 1 つだけ作る。
class LastResortArming final
{
  public:
    // backup は全部書けたら真を返すが、手続きは値を使わない（試験と controller の契約のため）。
    explicit LastResortArming(std::function<bool()> backup);
    ~LastResortArming();
    LastResortArming(const LastResortArming &) = delete;
    LastResortArming(LastResortArming &&) = delete;
    LastResortArming &operator=(const LastResortArming &) = delete;
    LastResortArming &operator=(LastResortArming &&) = delete;
};

// 呼んだスレッド（ワーカー）に同じ手続きを置く。このスレッドで呼ばれた手続きは退避せずに前の手続きへ
// 渡す。
void arm_last_resort_thread() noexcept;
} // namespace nenenib::adapters::win32
