#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "VimRecordedKeys.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::core::Column;
using nenenib::core::LineNumber;
using nenenib::core::TextPosition;
using nenenib::core::VimCharacter;
using nenenib::core::VimKey;
using nenenib::core::VimRecordedKeys;
using nenenib::core::VimSearchDirection;
using nenenib::core::VimSearchPattern;
using nenenib::core::VimSpecialKey;

static_assert(!std::is_default_constructible_v<VimRecordedKeys>);
static_assert(std::is_copy_constructible_v<VimRecordedKeys>);
static_assert(std::is_copy_assignable_v<VimRecordedKeys>);

[[nodiscard]] std::vector<VimKey> sample_keys(std::size_t count)
{
    std::vector<VimKey> keys;
    keys.reserve(count);
    for (std::size_t at = 0; at < count; ++at)
    {
        keys.emplace_back(VimCharacter{U'A' + static_cast<char32_t>(at)});
    }
    return keys;
}

void verify_key_boundaries()
{
    for (const std::size_t count : std::array<std::size_t, 8>{0, 1, 63, 64, 65, 127, 128, 129})
    {
        const auto expected = sample_keys(count);
        const auto snapshot = VimRecordedKeys::from(expected);
        expect(snapshot.owned_keys() == expected, "factory preserves order across chunk edges");
        auto appended = VimRecordedKeys::from({});
        for (const auto &key : expected)
        {
            appended = appended.appended(key);
        }
        expect(appended.owned_keys() == expected, "append preserves order across chunk edges");
        auto left_expected = expected;
        left_expected.emplace_back(VimSpecialKey::escape);
        auto right_expected = expected;
        right_expected.emplace_back(VimSpecialKey::enter);
        const auto left = snapshot.appended(VimKey{VimSpecialKey::escape});
        const auto right = snapshot.appended(VimKey{VimSpecialKey::enter});
        expect(left.owned_keys() == left_expected && right.owned_keys() == right_expected,
               "two branches retain their distinct appended keys");
        expect(snapshot.owned_keys() == expected && appended.owned_keys() == expected,
               "branching cannot change either earlier snapshot");
    }
}

void verify_key_ownership()
{
    const std::vector<VimKey> expected{VimSearchPattern{std::string(96, 'p'),
                                                        VimSearchDirection::backward,
                                                        TextPosition{LineNumber{3}, Column{7}}},
                                       VimCharacter{U'あ'}, VimSpecialKey::control_r};
    auto input = expected;
    char *pattern_alias = std::get<VimSearchPattern>(input.at(0)).pattern.data();
    const auto snapshot = VimRecordedKeys::from(input);
    pattern_alias[17] = 'x';
    std::get<VimSearchPattern>(input.at(0)).direction = VimSearchDirection::forward;
    std::get<VimSearchPattern>(input.at(0)).from = std::nullopt;
    input.clear();
    expect(snapshot.owned_keys() == expected,
           "factory defensively owns patterns, direction, origin and heterogeneous keys");
    auto returned = snapshot.owned_keys();
    std::get<VimSearchPattern>(returned.at(0)).pattern[29] = 'y';
    returned.at(1) = VimSpecialKey::backspace;
    expect(snapshot.owned_keys() == expected, "flattening returns an independent owning vector");
    VimKey added = VimSearchPattern{std::string(96, 'q'), VimSearchDirection::forward,
                                    TextPosition{LineNumber{4}, Column{2}}};
    auto appended_expected = expected;
    appended_expected.push_back(added);
    char *added_alias = std::get<VimSearchPattern>(added).pattern.data();
    const auto next = snapshot.appended(added);
    added_alias[31] = 'z';
    std::get<VimSearchPattern>(added).from = std::nullopt;
    expect(next.owned_keys() == appended_expected && snapshot.owned_keys() == expected,
           "append defensively copies an external key without mutating the old snapshot");
}

void verify_key_rvalue_copies()
{
    for (const std::size_t count : std::array<std::size_t, 3>{0, 64, 65})
    {
        const auto expected = sample_keys(count);
        auto source = VimRecordedKeys::from(expected);
        // copy-only snapshot の非 const xvalue を試す。std::move 後の観測は pinned tidy
        // が拒否する。
        auto constructed = static_cast<VimRecordedKeys &&>(source);
        expect(source.owned_keys() == expected && constructed.owned_keys() == expected,
               "rvalue construction shares ownership while leaving the source valid");
        auto assigned = VimRecordedKeys::from({}).appended(VimKey{VimSpecialKey::escape});
        assigned = static_cast<VimRecordedKeys &&>(source);
        expect(source.owned_keys() == expected && assigned.owned_keys() == expected,
               "rvalue assignment also preserves both values, including empty transitions");
        auto changed = assigned.appended(VimKey{VimSpecialKey::enter});
        auto changed_expected = expected;
        changed_expected.emplace_back(VimSpecialKey::enter);
        expect(changed.owned_keys() == changed_expected && source.owned_keys() == expected &&
                   constructed.owned_keys() == expected && assigned.owned_keys() == expected,
               "copies stay valid when a later branch replaces or adds a tail");
    }
}
} // namespace

void verify_vim_recorded_keys_scope()
{
    verify_key_boundaries();
    verify_key_ownership();
    verify_key_rvalue_copies();
}
} // namespace nenenib::tests
