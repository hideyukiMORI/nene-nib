#pragma once

#include "Milestone.hpp"

namespace nenenib::application
{
// 速さの節目を受け取る唯一の入口（ADR 0011 の決定 1）。窓はここへ打つだけで時刻を知らない。
// 実装は src/adapters/win32 だけが持つ（ARC-003 / ARC-007）。記録しない状態も同じ型が持つ。
class TimingPort
{
  public:
    TimingPort() = default;
    virtual ~TimingPort() = default;
    TimingPort(const TimingPort &) = delete;
    TimingPort(TimingPort &&) = delete;
    TimingPort &operator=(const TimingPort &) = delete;
    TimingPort &operator=(TimingPort &&) = delete;

    virtual void mark(core::Milestone milestone) noexcept = 0;
};
} // namespace nenenib::application
