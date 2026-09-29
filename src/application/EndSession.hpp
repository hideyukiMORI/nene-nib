#pragma once

#include "SessionEnd.hpp"

namespace nenenib::application
{
// 窓を壊す直前に ui が送る意図（ADR 0059 の決定 3）。controller は状態から前回のタブの一覧を作って
// SessionPort::write へ渡す。文書もタブも変えず、書けなくても何も出さない。
struct EndSession
{
    SessionEnd reason;
};
} // namespace nenenib::application
