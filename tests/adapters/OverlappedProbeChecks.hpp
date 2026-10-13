#pragma once
#include <cstddef>
#include <cstdio>
#include <mutex>
namespace nenenib::tests
{
class OverlappedProbeChecks final
{
  public:
    void expect(bool condition, const char *message)
    {
        const std::scoped_lock lock(mutex_);
        ++checks_;
        if (!condition)
        {
            ++failures_;
            std::printf("FAIL: %s\n", message);
        }
    }
    [[nodiscard]] int finish() const
    {
        const std::scoped_lock lock(mutex_);
        std::printf("OVERLAPPED probe: %zu checks, %zu failures\n", checks_, failures_);
        return failures_ == 0 ? 0 : 1;
    }

  private:
    mutable std::mutex mutex_;
    std::size_t checks_ = 0, failures_ = 0;
};
} // namespace nenenib::tests
