#pragma once

#include "LastResortHooks.hpp"

namespace nenenib::adapters::win32
{
// 最後の砦で終えるときの終了コード（ADR 0137 の追補 #1625・1 か所）。run の 0 / 1、abort の 3、
// __fastfail の 0xC0000409 と重ならない値（Issue の番号）。
inline constexpr unsigned int last_resort_exit_code = 0x1625U;

// 最後の砦（ADR 0137 の追補 #1625）。捕まえていない C++ の例外と、noexcept を抜けた例外で呼ばれる
// std::terminate の手続きを 1 つ置く。std::set_terminate は呼んだスレッドだけに効く（UCRT は
// スレッドごとの欄に持つ）ので、UI スレッドはこの型を作ったときに、ワーカーは
// arm_last_resort_thread を最初の仕事の頭で呼んで、同じ手続きを置く。
// 手続きは、作ったスレッド（UI）で呼ばれたときだけ hooks.backup を 1 回呼び、そのスレッドの窓の
// 手続きを止めてから（窓の手続きを DefWindowProcW に替えて入力を止める・壊れた状態で回さない）
// hooks.notice で知らせ、last_resort_exit_code で process を終える（TerminateProcess・
// DestroyWindow も後始末も通らない）。ほかのスレッドで呼ばれたら何もせずに終える。2 度目に
// 入ったら（退避の中でまた例外・別のスレッドが同時に）すぐ終える。SEH（アクセス違反・stack の
// 溢れ）は受けない。壊すと手続きを外す（hooks は持ち主の controller より先に消える）。合成ルートが
// 起動のときに 1 つだけ作る。
class LastResortArming final
{
  public:
    explicit LastResortArming(LastResortHooks hooks);
    ~LastResortArming();
    LastResortArming(const LastResortArming &) = delete;
    LastResortArming(LastResortArming &&) = delete;
    LastResortArming &operator=(const LastResortArming &) = delete;
    LastResortArming &operator=(LastResortArming &&) = delete;
};

// 呼んだスレッド（ワーカー）に同じ手続きを置く。このスレッドで呼ばれた手続きは退避せずに終える。
void arm_last_resort_thread() noexcept;
} // namespace nenenib::adapters::win32
