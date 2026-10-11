#pragma once

#include <cstdint>
#include <optional>

namespace nenenib::application
{
struct ScreenMotion
{
    std::int64_t rows;
    std::optional<float> x;
};
} // namespace nenenib::application
