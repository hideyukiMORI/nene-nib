#pragma once

#include "FilePath.hpp"

namespace nenenib::adapters::win32
{
struct SettingsPaths
{
    core::FilePath current;
    core::FilePath previous;
};
} // namespace nenenib::adapters::win32
