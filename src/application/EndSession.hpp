#pragma once

#include "SessionEnd.hpp"

namespace nenenib::application
{
// 窓を壊す直前に ui が送る意図（ADR 0059 の決定 3）。controller は状態から前回のタブの一覧を作って
// SessionPort::write へ渡す。文書もタブも変えず、書けなくても何も出さない。
// unsaved_discarded は、使う人が「保存しますか」に「いいえ」と答えて未保存を捨てて閉じたこと
// （ADR 0137 の決定 5）。確かめずに閉じる OS の終了（WM_ENDSESSION）では偽のまま。
struct EndSession
{
    SessionEnd reason;
    bool unsaved_discarded = false;
};
} // namespace nenenib::application
