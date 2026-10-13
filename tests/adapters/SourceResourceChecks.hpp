#pragma once
#include <cstddef>
#include <cstdio>
namespace nenenib::tests
{
class SourceResourceChecks final
{
  public:
    void expect(bool condition, const char *message)
    {
        ++checks_;
        if (!condition)
        {
            ++failures_;
            std::printf("FAIL: %s\n", message);
        }
    }
    [[nodiscard]] int finish() const
    {
        std::printf("Source resource probe: %zu checks, %zu failures\n", checks_, failures_);
        return failures_ == 0 ? 0 : 1;
    }

  private:
    std::size_t checks_ = 0;
    std::size_t failures_ = 0;
};
} // namespace nenenib::tests
