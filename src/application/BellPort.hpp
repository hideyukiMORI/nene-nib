#pragma once

namespace nenenib::application
{
// 断りの音の唯一の入口（ADR 0153 の決定 4）。鳴らすかは controller の reject の 1 か所が決め、
// 音を出すのは実装（src/adapters/win32）だけ（ARC-007）。音の種類は 1 つで、結果は返さない。
class BellPort
{
  public:
    BellPort() = default;
    virtual ~BellPort() = default;
    BellPort(const BellPort &) = delete;
    BellPort(BellPort &&) = delete;
    BellPort &operator=(const BellPort &) = delete;
    BellPort &operator=(BellPort &&) = delete;

    virtual void ring() noexcept = 0;
};
} // namespace nenenib::application
