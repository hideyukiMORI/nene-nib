#pragma once

#include "EasyMotionKind.hpp"

namespace nenenib::application
{
// イージーモーションの `\f` `\F` `\s` の 1 文字の入力行（ADR 0163 の決定 4）。CommandInput の
// 1 値で、engine が跳び先の文字を待つ間（input_wait が EasyMotionKind で easy_motion が無い）だけ
// 開く。IME は使う人に任せ（ImeStance::as_left）、確定した最初の 1 文字を跳び先の文字として engine
// へ流して閉じる。kind は待っている動作（見え方には使わない）。値は単独で妥当な公開 aggregate。
struct JumpCharacter
{
    core::EasyMotionKind kind;
};
} // namespace nenenib::application
