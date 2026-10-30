#pragma once

#include "ShellRegistrationEntry.hpp"

#include <vector>

namespace nenenib::core
{
// エクスプローラーへの登録の鍵と値を書く順に並べたもの（ADR 0178 の決定 2）。作るのは
// shell_registration_plan の 1 本で、port の apply / remove / status は同じ計画を受ける（決定 1）。
using ShellRegistrationPlan = std::vector<ShellRegistrationEntry>;
} // namespace nenenib::core
