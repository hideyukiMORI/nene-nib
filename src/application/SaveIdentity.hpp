#pragma once

#include <cstdint>

namespace nenenib::application
{
enum class SaveIdentity : std::uint8_t
{
    update_document,
    retain_document
};
} // namespace nenenib::application
