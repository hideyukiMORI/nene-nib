#pragma once

#include <cstdint>

namespace nenenib::application
{
// エクスプローラーへの登録の今の状態（ADR 0178 の決定 1）。registered_elsewhere は登録はあるが
// command の exe のパスが計画と違う（exe を動かした後）。読めないときは absent に畳む。
enum class ShellRegistrationStatus : std::uint8_t
{
    registered,
    registered_elsewhere,
    absent
};
} // namespace nenenib::application
