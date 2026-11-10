#pragma once

#include <utility>

namespace nenenib::application
{
// 欄 target を value に置き、壊れるときに元の値へ戻す（例外で抜けても戻る・#1312）。controller が
// `map` の右辺を流す間だけ鍵の出どころと録画に積む左辺を置くのに使う。写しも移しもしない。
template <typename Value> class ScopedValue final
{
  public:
    ScopedValue(Value &target, Value value) : target_(&target), saved_(std::exchange(target, value))
    {
    }
    ScopedValue(const ScopedValue &) = delete;
    ScopedValue(ScopedValue &&) = delete;
    ScopedValue &operator=(const ScopedValue &) = delete;
    ScopedValue &operator=(ScopedValue &&) = delete;
    ~ScopedValue()
    {
        *target_ = std::move(saved_);
    }

  private:
    Value *target_;
    Value saved_;
};
} // namespace nenenib::application
