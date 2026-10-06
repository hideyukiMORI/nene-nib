#include "WindowChecks.hpp"

#include <cstdio>

namespace nenenib::tests::ui
{
namespace
{
std::size_t &checked()
{
    static std::size_t count = 0;
    return count;
}

std::size_t &failed()
{
    static std::size_t count = 0;
    return count;
}

std::size_t &skipped()
{
    static std::size_t count = 0;
    return count;
}
} // namespace

void expect(bool condition, const char *message)
{
    ++checked();
    if (!condition)
    {
        ++failed();
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}

void unmeasured(const char *what)
{
    ++skipped();
    std::printf("not measured (environment): %s\n", what);
}

std::size_t checks()
{
    return checked();
}

std::size_t failures()
{
    return failed();
}

std::size_t unmeasured_count()
{
    return skipped();
}
} // namespace nenenib::tests::ui
