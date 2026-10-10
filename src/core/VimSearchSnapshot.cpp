#include "VimSearchSnapshot.hpp"

#include <utility>

namespace nenenib::core
{
VimSearchSnapshot::VimSearchSnapshot(std::shared_ptr<const Storage> storage)
    : storage_(std::move(storage))
{
}

VimSearchSnapshot VimSearchSnapshot::from(std::string_view text, VimSearchDirection direction)
{
    std::string owned{text};
    auto parsed = VimPattern::parse(owned, direction);
    return VimSearchSnapshot(
        std::make_shared<const Storage>(Storage{std::move(owned), direction, std::move(parsed)}));
}

std::string_view VimSearchSnapshot::text() const & noexcept
{
    return storage_->text;
}

VimSearchDirection VimSearchSnapshot::direction() const noexcept
{
    return storage_->direction;
}

const std::expected<VimPattern, VimPatternFailure> &VimSearchSnapshot::parsed() const & noexcept
{
    return storage_->parsed;
}
} // namespace nenenib::core
