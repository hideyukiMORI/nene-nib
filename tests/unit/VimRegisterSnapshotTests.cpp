#include "Editing.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "VimRegisterSnapshot.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"

#include <array>
#include <string>
#include <type_traits>
#include <utility>

namespace nenenib::tests
{
namespace
{
using core::VimRegister;
using core::VimRegisterKind;
using core::VimRegisterSnapshot;

template <typename T>
concept RvalueRegisterBorrow = requires(T &&snapshot) { std::move(snapshot).value(); };
static_assert(!RvalueRegisterBorrow<VimRegisterSnapshot>);
static_assert(!std::is_default_constructible_v<VimRegisterSnapshot>);
static_assert(!std::is_convertible_v<VimRegister, VimRegisterSnapshot>);
static_assert(std::is_same_v<decltype(std::declval<const VimRegisterSnapshot &>().value()),
                             const VimRegister &>);

void expect_register(const VimRegister &actual, const VimRegister &expected)
{
    expect(actual.text == expected.text, "snapshot keeps the complete text");
    expect(actual.kind == expected.kind, "snapshot keeps the exact kind");
    expect(actual.width == expected.width, "snapshot keeps the exact width");
}

void verify_snapshot_value(const VimRegister &raw)
{
    const auto frozen = VimRegisterSnapshot::from(raw);
    expect_register(frozen.value(), raw);
    const auto copied = frozen;
    expect_register(copied.value(), raw);
    if (!raw.text.empty())
    {
        expect(&copied.value() == &frozen.value(), "nonempty copies share the const payload");
    }
}

void verify_snapshot_values()
{
    constexpr std::array kinds{VimRegisterKind::uninitialized, VimRegisterKind::characters,
                               VimRegisterKind::lines, VimRegisterKind::block};
    for (VimRegisterKind kind : kinds)
    {
        for (const std::string &text :
             std::array{std::string{}, std::string{"short"}, std::string(4096, 'a')})
        {
            verify_snapshot_value(VimRegister{text, kind, core::VimBlockWidth{3}});
        }
    }
}

void verify_defensive_snapshot()
{
    VimRegister raw{std::string(4096, 'a'), VimRegisterKind::block, core::VimBlockWidth{7}};
    const VimRegister expected = raw;
    char *alias = raw.text.data();
    auto &kind_alias = raw.kind;
    auto &width_alias = raw.width;
    const auto frozen = VimRegisterSnapshot::from(std::move(raw));
    alias[0] = 'z';
    kind_alias = VimRegisterKind::lines;
    width_alias = std::nullopt;
    expect_register(frozen.value(), expected);
    expect(alias[0] == 'z', "the external mutable alias remains external");
}

void verify_snapshot_rvalues()
{
    auto source =
        VimRegisterSnapshot::from(VimRegister{std::string(4096, 'r'), VimRegisterKind::characters});
    const VimRegister expected = source.value();
    // copy-only型のxvalueでも元を再観測する。std::moveの字句で落とすtidyとは別に検証する。
    const auto constructed = static_cast<VimRegisterSnapshot &&>(source);
    expect_register(source.value(), expected);
    expect_register(constructed.value(), expected);
    expect(&source.value() == &constructed.value(), "rvalue construction retains both owners");
    auto assigned = VimRegisterSnapshot::from(VimRegister{"", VimRegisterKind::lines});
    assigned = static_cast<VimRegisterSnapshot &&>(source);
    expect_register(source.value(), expected);
    expect_register(assigned.value(), expected);
    expect(&source.value() == &assigned.value(), "rvalue assignment retains both owners");
    auto empty =
        VimRegisterSnapshot::from(VimRegister{"", VimRegisterKind::block, core::VimBlockWidth{5}});
    const auto empty_copy = static_cast<VimRegisterSnapshot &&>(empty);
    expect_register(empty_copy.value(), empty.value());
}

void verify_state_branches()
{
    const VimRegister held{std::string(4096, 'h'), VimRegisterKind::characters};
    auto original = core::vim_resting_state(held);
    original = core::vim_register_stored(original, 'a', held);
    original = core::vim_register_stored(original, '1', held);
    original = core::vim_register_stored(original, '-', held);
    original = core::vim_clipboard_loaded(original, held);
    const auto sibling = original;
    auto changed = core::vim_register_stored(original, 'A', VimRegister{"tail", held.kind});
    changed = core::vim_register_stored(changed, '1', VimRegister{"number", held.kind});
    changed = core::vim_register_stored(changed, '-', VimRegister{"small", held.kind});
    changed = core::vim_clipboard_loaded(changed, VimRegister{"clipboard", held.kind});
    changed.unnamed_register = VimRegisterSnapshot::from(VimRegister{"unnamed", held.kind});
    for (const auto &state : std::array{original, sibling})
    {
        expect_register(state.unnamed_register.value(), held);
        expect_register(state.registers.registers.at(0).value(), held);
        expect_register(state.numbered.registers.at(1).value(), held);
        expect_register(state.small_delete.value(), held);
        expect_register(state.clipboard.value(), held);
    }
    expect(changed.registers.registers.at(0).value().text == held.text + "tail",
           "append changes only the new named snapshot");
    expect(changed.numbered.registers.at(1).value().text == "number", "new numbered slot changes");
    expect(changed.small_delete.value().text == "small", "new small delete changes");
    expect(changed.clipboard.value().text == "clipboard", "new clipboard changes");
    expect(changed.unnamed_register.value().text == "unnamed", "new unnamed changes");
    const auto resting = core::vim_resting_from(original, original.unnamed_register);
    expect(&resting.unnamed_register.value() == &original.unnamed_register.value(),
           "resting carries the existing owner without refreezing");
    expect_register(resting.clipboard.value(), VimRegister{"", VimRegisterKind::uninitialized});
    expect_register(original.clipboard.value(), held);
}

void verify_writer_sharing()
{
    Editing editing;
    open_vim_document(editing, "one\ntwo\nthree");
    vim_replay(editing.controller(), "\"add");
    const auto first = editing.controller().vim_state();
    expect(first.unnamed_register.value().text == "one\n", "named delete keeps its previous text");
    expect(&first.unnamed_register.value() == &first.registers.registers.at(0).value() &&
               &first.unnamed_register.value() == &first.numbered.registers.at(1).value(),
           "one delete is frozen once for named, numbered and unnamed");
    vim_replay(editing.controller(), "dd");
    const auto &second = editing.controller().vim_state();
    expect(&second.numbered.registers.at(2).value() == &first.numbered.registers.at(1).value(),
           "numbered shifting shares the old immutable payload");
    expect(first.unnamed_register.value().text == "one\n" &&
               first.numbered.registers.at(1).value().text == "one\n",
           "the previous state survives the next delete");
    Editing small;
    open_vim_document(small, "abc");
    vim_replay(small.controller(), "x");
    const auto &state = small.controller().vim_state();
    expect(&state.unnamed_register.value() == &state.small_delete.value(),
           "small delete and unnamed share one frozen edit");
}
} // namespace

void verify_vim_register_snapshot_contracts()
{
    verify_snapshot_values();
    verify_defensive_snapshot();
    verify_snapshot_rvalues();
    verify_state_branches();
    verify_writer_sharing();
}
} // namespace nenenib::tests
