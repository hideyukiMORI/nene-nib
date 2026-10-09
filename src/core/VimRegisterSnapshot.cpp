#include "VimRegisterSnapshot.hpp"

#include <memory>
#include <utility>
#include <variant>

namespace nenenib::core
{
VimRegisterSnapshot::VimRegisterSnapshot(Storage value) : value_(std::move(value)) {}

VimRegisterSnapshot VimRegisterSnapshot::from(const VimRegister &value)
{
    if (value.text.empty())
    {
        return VimRegisterSnapshot(value);
    }
    return VimRegisterSnapshot(std::make_shared<const VimRegister>(value));
}

const VimRegister &VimRegisterSnapshot::value() const & noexcept
{
    if (const auto *empty = std::get_if<VimRegister>(&value_))
    {
        return *empty;
    }
    return *std::get<std::shared_ptr<const VimRegister>>(value_);
}
} // namespace nenenib::core
