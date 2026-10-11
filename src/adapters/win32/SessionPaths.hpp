#pragma once

#include "FilePath.hpp"

namespace nenenib::adapters::win32
{
// v2が無いときだけv1を読む。終了時の保存先は常にcurrent。
struct SessionPaths
{
    core::FilePath current;
    core::FilePath legacy;
};
} // namespace nenenib::adapters::win32
