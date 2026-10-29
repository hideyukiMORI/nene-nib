#include "VimSearchSnapshot.hpp"

#include <utility>

namespace nenenib::core
{
VimSearchSnapshot::VimSearchSnapshot(std::shared_ptr<const Storage> storage)
    : storage_(std::move(storage))
{
}

namespace
{
// `*` `#` は smartcase を見ず ignorecase だけを見る（Vim の no_scs・実測）。
[[nodiscard]] VimPatternCase applied_case(VimPatternCase letter_case,
                                          VimSmartcase smartcase) noexcept
{
    switch (smartcase)
    {
    case VimSmartcase::honored:
        return letter_case;
    case VimSmartcase::ignored:
        return letter_case == VimPatternCase::smart ? VimPatternCase::ignore : letter_case;
    }
    std::unreachable();
}
} // namespace

VimSearchSnapshot VimSearchSnapshot::from(std::string_view text, VimSearchDirection direction,
                                          VimPatternCase letter_case, VimSmartcase smartcase)
{
    std::string owned{text};
    auto parsed = VimPattern::parse(owned, direction, applied_case(letter_case, smartcase));
    return VimSearchSnapshot(std::make_shared<const Storage>(
        Storage{std::move(owned), direction, smartcase, std::move(parsed)}));
}

VimSearchSnapshot VimSearchSnapshot::recased(VimPatternCase letter_case) const
{
    return from(storage_->text, storage_->direction, letter_case, storage_->smartcase);
}

VimSmartcase VimSearchSnapshot::smartcase() const noexcept
{
    return storage_->smartcase;
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
