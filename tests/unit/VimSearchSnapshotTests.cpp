#include "Editing.hpp"
#include "NewTab.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "VimSearchSnapshot.hpp"
#include "VimTestSupport.hpp"

#include <array>
#include <string>
#include <type_traits>
#include <utility>

namespace nenenib::tests
{
namespace
{
using core::VimSearchDirection;
using core::VimSearchSnapshot;

template <typename T>
concept RvalueSearchText = requires(T &&snapshot) { std::move(snapshot).text(); };
template <typename T>
concept RvalueSearchParsed = requires(T &&snapshot) { std::move(snapshot).parsed(); };
static_assert(!RvalueSearchText<VimSearchSnapshot>);
static_assert(!RvalueSearchParsed<VimSearchSnapshot>);
static_assert(!std::is_default_constructible_v<VimSearchSnapshot>);
static_assert(!std::is_convertible_v<std::string, VimSearchSnapshot>);
static_assert(std::is_same_v<decltype(std::declval<const VimSearchSnapshot &>().parsed()),
                             const std::expected<core::VimPattern, core::VimPatternFailure> &>);

void expect_search_literal(const VimSearchSnapshot &snapshot, std::string_view text)
{
    expect(snapshot.text() == text && snapshot.direction() == VimSearchDirection::forward,
           "the snapshot owns the exact raw text and direction");
    const auto &parsed = snapshot.parsed();
    expect(parsed.has_value(), "the remembered literal parses");
    if (!parsed.has_value())
    {
        return;
    }
    const auto match = parsed.value().matched(text, 0U);
    expect(match.has_value() && match.value().begin == 0U && match.value().end == text.size(),
           "the stored parse still matches the original complete literal");
}

void verify_search_defensive_ownership()
{
    std::string raw(256U, 'a');
    const std::string expected = raw;
    char *alias = raw.data();
    const auto snapshot = VimSearchSnapshot::from(raw, VimSearchDirection::forward);
    alias[0] = 'z';
    expect_search_literal(snapshot, expected);
    expect(raw.front() == 'z', "the caller keeps its independent mutable buffer");
    raw.clear();
    raw.shrink_to_fit();
    expect_search_literal(snapshot, expected);
    const auto temporary =
        VimSearchSnapshot::from(std::string("short"), VimSearchDirection::forward);
    expect_search_literal(temporary, "short");
}

void verify_search_copy_and_rvalues()
{
    auto original = VimSearchSnapshot::from("alpha", VimSearchDirection::forward);
    const auto copied = original;
    const auto constructed = static_cast<VimSearchSnapshot &&>(original);
    auto assigned = VimSearchSnapshot::from("other", VimSearchDirection::backward);
    assigned = static_cast<VimSearchSnapshot &&>(original);
    for (const auto &snapshot : std::array{original, copied, constructed, assigned})
    {
        expect_search_literal(snapshot, "alpha");
        expect(&snapshot.parsed() == &original.parsed(), "all owners share the same const parse");
    }
    original = VimSearchSnapshot::from("changed", VimSearchDirection::forward);
    expect_search_literal(original, "changed");
    expect_search_literal(copied, "alpha");
    expect(&original.parsed() != &copied.parsed(), "replacing an owner leaves the sibling intact");
}

void expect_search_failure(std::string_view raw, core::VimPatternFailure expected)
{
    const auto snapshot = VimSearchSnapshot::from(raw, VimSearchDirection::forward);
    const auto copy = snapshot;
    expect(copy.text() == raw && !copy.parsed().has_value() && copy.parsed().error() == expected,
           "a failed parse keeps its raw value and exact closed failure");
    expect(&copy.parsed() == &snapshot.parsed(), "failed parse copies share the immutable result");
}

void verify_search_failures_and_direction()
{
    using core::VimPatternFailure;
    expect_search_failure("\\(", VimPatternFailure::group);
    expect_search_failure("a\\|b", VimPatternFailure::branch);
    expect_search_failure("a\\+", VimPatternFailure::quantifier);
    expect_search_failure("\\va", VimPatternFailure::magic);
    expect_search_failure("\\ca", VimPatternFailure::ignore_case);
    expect_search_failure("\\n", VimPatternFailure::escape);
    expect_search_failure("~", VimPatternFailure::previous_substitute);
    expect_search_failure("a/e", VimPatternFailure::offset);
    const auto forward = VimSearchSnapshot::from("a?", VimSearchDirection::forward);
    expect_search_literal(forward, "a?");
    const auto backward = VimSearchSnapshot::from(forward.text(), VimSearchDirection::backward);
    expect(backward.text() == "a?" && backward.direction() == VimSearchDirection::backward &&
               !backward.parsed().has_value() &&
               backward.parsed().error() == VimPatternFailure::offset,
           "the same raw text is reparsed for the new delimiter direction");
    const auto restored = VimSearchSnapshot::from(backward.text(), VimSearchDirection::forward);
    expect_search_literal(restored, "a?");
    const auto ranges = VimSearchSnapshot::from("[日界]*x", VimSearchDirection::forward);
    expect(ranges.parsed().has_value() && ranges.parsed().value().matched("日界x", 0U).has_value(),
           "owned set ranges and repeated atoms remain available to the one evaluator");
}

void verify_search_state_branches()
{
    auto original =
        core::vim_resting_state(core::VimRegister{"", core::VimRegisterKind::characters});
    original.last_search = VimSearchSnapshot::from("before", VimSearchDirection::forward);
    const auto sibling = original;
    auto changed = core::vim_resting_from(original, original.unnamed_register);
    expect(changed.last_search.has_value() && original.last_search.has_value() &&
               &changed.last_search.value().parsed() == &original.last_search.value().parsed(),
           "resting carries the existing owner without parsing again");
    changed.last_search = VimSearchSnapshot::from("after", VimSearchDirection::backward);
    expect(original.last_search.has_value() && sibling.last_search.has_value() &&
               original.last_search.value().text() == "before" &&
               &original.last_search.value().parsed() == &sibling.last_search.value().parsed(),
           "two old state branches retain the original search");
    expect(changed.last_search.value().text() == "after" &&
               changed.last_search.value().direction() == VimSearchDirection::backward,
           "only the new state branch changes its remembered search");
}

void verify_search_empty_direction_reuse()
{
    Editing editing;
    open_vim_document(editing, "a? x a?");
    auto &controller = editing.controller();
    vim_replay(controller, "/a?<CR>");
    expect(caret_at(controller.frame(), 1U, 6U), "the forward question mark is a literal");
    const auto forward = controller.vim_state().last_search;
    vim_replay(controller, "?<CR>");
    const auto backward = controller.vim_state().last_search;
    expect(backward.has_value() && backward.value().text() == "a?" &&
               backward.value().direction() == VimSearchDirection::backward &&
               !backward.value().parsed().has_value() &&
               backward.value().parsed().error() == core::VimPatternFailure::offset,
           "empty backward confirmation remembers its newly parsed failure");
    expect(message_is(controller.frame(), "Search offset is not supported") &&
               controller.frame().lines.front().matches.empty() &&
               caret_at(controller.frame(), 1U, 6U),
           "the failed direction neither moves nor paints the previous successful parse");
    vim_replay(controller, "n");
    expect(message_is(controller.frame(), "Search offset is not supported"),
           "repeat reads the same remembered failure");
    vim_replay(controller, "/<CR>");
    expect(caret_at(controller.frame(), 1U, 1U) &&
               controller.vim_state().last_search.value().parsed().has_value() &&
               controller.frame().lines.front().matches.size() == 2U,
           "empty forward confirmation reparses the raw text back into a success");
    expect(forward.has_value() && forward.value().parsed().has_value() &&
               !backward.value().parsed().has_value(),
           "saved success and failure owners survive later search confirmations");
}

void verify_search_typing_does_not_fall_back()
{
    Editing editing;
    open_vim_document(editing, "a x a");
    auto &controller = editing.controller();
    vim_replay(controller, "/a<CR>");
    const auto remembered = controller.vim_state().last_search;
    vim_replay(controller, "/\\(");
    const auto invalid = controller.frame();
    expect(invalid.command_line.has_value() && invalid.lines.front().matches.empty() &&
               !invalid.lines.front().current_match.has_value(),
           "invalid input owns no pattern and does not borrow the previous confirmed value");
    expect(remembered.has_value() && controller.vim_state().last_search.has_value() &&
               &remembered.value().parsed() == &controller.vim_state().last_search.value().parsed(),
           "typing does not replace the remembered owner");
    vim_replay(controller, "<Esc>");
    expect(controller.frame().lines.front().matches.size() == 2U,
           "cancelling restores the confirmed pattern's two spans");
}

void verify_search_long_word_and_tabs()
{
    Editing editing;
    const std::string word(4096U, 'a');
    open_vim_document(editing, word + " " + word);
    auto &controller = editing.controller();
    vim_replay(controller, "*");
    expect(caret_at(controller.frame(), 1U, 4098U), "word search reaches the second long word");
    const auto remembered = controller.vim_state().last_search;
    expect(remembered.has_value() && remembered.value().text() == "\\<" + word + "\\>" &&
               remembered.value().parsed().has_value(),
           "word search owns its generated 4100-byte pattern beyond the typed input limit");
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::ordinary}));
    static_cast<void>(controller.apply(application::NewTab{}));
    expect(controller.vim_state().last_search.has_value() &&
               &controller.vim_state().last_search.value().parsed() == &remembered.value().parsed(),
           "ordinary mode and a new tab retain the same window search owner");
    static_cast<void>(controller.apply(application::SwitchTab{0U}));
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::vim}));
    vim_replay(controller, "#");
    expect(caret_at(controller.frame(), 1U, 1U) &&
               controller.vim_state().last_search.value().direction() ==
                   VimSearchDirection::backward &&
               controller.vim_state().last_search.value().text() == remembered.value().text(),
           "backward word search creates the same bounded raw value in its own direction");
    expect(remembered.value().direction() == VimSearchDirection::forward,
           "the previous long owner survives the later backward confirmation");
}
} // namespace

void verify_vim_search_snapshot_contracts()
{
    verify_search_defensive_ownership();
    verify_search_copy_and_rvalues();
    verify_search_failures_and_direction();
    verify_search_state_branches();
    verify_search_empty_direction_reuse();
    verify_search_typing_does_not_fall_back();
    verify_search_long_word_and_tabs();
}
} // namespace nenenib::tests
