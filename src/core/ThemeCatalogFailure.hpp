#pragma once

#include <cstdint>

namespace nenenib::core
{
enum class ThemeCatalogFailure : std::uint8_t
{
    reserved_name,
    duplicate_name,
    name_mismatch
};
} // namespace nenenib::core
