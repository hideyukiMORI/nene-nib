#pragma once

#include "VimRegister.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <utility>
#include <variant>

namespace nenenib::core
{
class VimRegisterSnapshot final
{
  public:
    VimRegisterSnapshot(const VimRegisterSnapshot &) = default;
    VimRegisterSnapshot &operator=(const VimRegisterSnapshot &) = default;

    [[nodiscard]] static VimRegisterSnapshot from(const VimRegister &value);
    [[nodiscard]] const VimRegister &value() const & noexcept;
    const VimRegister &value() const && = delete;

  private:
    using Storage = std::variant<VimRegister, std::shared_ptr<const VimRegister>>;
    explicit VimRegisterSnapshot(Storage value);
    Storage value_;
};

// 空本文だけをinlineで作るので、空の表の初期化に本文のheap確保は要らない。
template <std::size_t... Indices>
[[nodiscard]] std::array<VimRegisterSnapshot, sizeof...(Indices)>
empty_vim_registers(std::index_sequence<Indices...>)
{
    return {(static_cast<void>(Indices),
             VimRegisterSnapshot::from(VimRegister{"", VimRegisterKind::uninitialized}))...};
}
} // namespace nenenib::core
