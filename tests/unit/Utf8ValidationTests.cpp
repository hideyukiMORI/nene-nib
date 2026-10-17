#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "Utf8.hpp"
#include "Utf8Validation.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nenenib::tests
{
namespace
{
using core::TextFailure;
using core::Utf8Validation;
using Result = std::expected<std::size_t, TextFailure>;

static_assert(!std::is_default_constructible_v<Utf8Validation>);
static_assert(std::is_copy_constructible_v<Utf8Validation>);

[[nodiscard]] Result validated_parts(std::span<const std::string_view> parts)
{
    auto progress = Utf8Validation::empty();
    for (const auto part : parts)
    {
        const auto next = progress.with_bytes(part);
        if (!next)
        {
            return std::unexpected(next.error());
        }
        progress = next.value();
    }
    return progress.finish();
}

// 期待値は既知の文字列から与える。一括入口だけをoracleにして同じ欠陥を見逃さない。
void expect_partitions(std::string_view text, Result expected)
{
    expect(core::validate_utf8(text) == expected, "the whole input matches the known fixture");
    for (std::size_t first = 0; first <= text.size(); ++first)
    {
        for (std::size_t second = first; second <= text.size(); ++second)
        {
            const std::array parts{text.substr(0, first), text.substr(first, second - first),
                                   text.substr(second)};
            expect(validated_parts(parts) == expected,
                   "every two- and three-part split matches the known fixture");
        }
    }
}

void verify_valid_partitions()
{
    constexpr std::array<std::pair<std::string_view, std::size_t>, 16> samples{{
        {"", 0},
        {"ASCII\r\n", 7},
        {std::string_view("A\0B\r\n", 5), 5},
        {"\xC2\x80", 1},
        {"\xDF\xBF", 1},
        {"\xE0\xA0\x80", 1},
        {"\xED\x9F\xBF", 1},
        {"\xEE\x80\x80", 1},
        {"\xEF\xBF\xBF", 1},
        {"\xF0\x90\x80\x80", 1},
        {"\xF4\x8F\xBF\xBF", 1},
        {"e\xCC\x81", 2},
        {"日本語", 3},
        {"a日\xF0\x9F\x99\x82"
         "b",
         4},
        {"\xEF\xBB\xBF", 1},
        {"\xEF\xBB\xBF"
         "A\r\n",
         4},
    }};
    for (const auto &[text, count] : samples)
    {
        expect_partitions(text, count);
    }
}

void verify_invalid_partitions()
{
    constexpr std::array<std::string_view, 23> samples{"\x80",
                                                       "\xBF",
                                                       "\xC0",
                                                       "\xC1",
                                                       "\xF5",
                                                       "\xFF",
                                                       "\xC0\x80",
                                                       "\xC1\xBF",
                                                       "\xE0\x80\x80",
                                                       "\xED\xA0\x80",
                                                       "\xED\xBF\xBF",
                                                       "\xF0\x80\x80\x80",
                                                       "\xF4\x90\x80\x80",
                                                       "\xF5\x80\x80\x80",
                                                       "\xE6\x28\xA5",
                                                       "\xF0\x9F\x28\x82",
                                                       "\xC2",
                                                       "\xE6\x97",
                                                       "\xF0\x9F\x99",
                                                       "a\xC2",
                                                       "\xE6\x97\xA5\x80",
                                                       "\xE6\x97"
                                                       "a",
                                                       "\xEF\xBB\xBF\xFF"};
    for (const auto text : samples)
    {
        expect_partitions(text, std::unexpected(TextFailure::invalid_utf8));
    }
}

void verify_empty_parts_and_finish()
{
    const auto empty = Utf8Validation::empty();
    expect(empty.finish() == Result{0}, "a new validation accepts an empty file");
    expect(empty.with_bytes("").value().finish() == Result{0}, "an empty part leaves it empty");
    const auto pending = empty.with_bytes("\xF0\x9F").value();
    expect(pending.finish() == Result{std::unexpected(TextFailure::invalid_utf8)},
           "only finish rejects an unfinished sequence at EOF");
    const auto unchanged = pending.with_bytes("").value();
    expect(unchanged.finish() == pending.finish(), "an empty part is not an EOF declaration");
    const auto complete = unchanged.with_bytes("\x99\x82").value();
    expect(complete.finish() == Result{1}, "an observed incomplete state can still continue");
    expect(complete.finish() == Result{1}, "finish is repeatable and does not consume the state");
    expect(complete.with_bytes("x").value().finish() == Result{2},
           "a finished prefix can also be extended");
    expect(pending.finish() == Result{std::unexpected(TextFailure::invalid_utf8)},
           "finishing another value does not mutate the old pending state");
}

void verify_branches_and_rejected_append()
{
    const auto base = Utf8Validation::empty().with_bytes("prefix:").value();
    const auto first = base.with_bytes("\xE6").value();
    const auto second = base.with_bytes("\xF0\x9F").value();
    expect(first.with_bytes("\x97\xA5").value().finish() == Result{8},
           "one branch completes its three-byte character");
    expect(second.with_bytes("\x99\x82").value().finish() == Result{8},
           "another branch completes its independent four-byte character");
    const auto rejected = first.with_bytes("x");
    expect(!rejected.has_value() && rejected.error() == TextFailure::invalid_utf8,
           "a non-continuation rejects an append immediately");
    expect(first.with_bytes("\x97\xA5").value().finish() == Result{8},
           "a rejected append preserves the original prefix for a different branch");
    expect(base.finish() == Result{7}, "neither branch changes the base count");
}

void verify_scalar_rejection_timing()
{
    constexpr std::array<std::string_view, 3> prefixes{"\xE0\x80", "\xED\xA0", "\xF4\x90\x80"};
    const auto empty = Utf8Validation::empty();
    for (const auto prefix : prefixes)
    {
        const auto appended = empty.with_bytes(prefix);
        expect(appended.has_value(), "scalar range is not rejected before the sequence completes");
        if (!appended)
        {
            continue;
        }
        const auto pending = appended.value();
        expect(pending.finish() == Result{std::unexpected(TextFailure::invalid_utf8)},
               "EOF rejects the unfinished scalar sequence");
        const auto rejected = pending.with_bytes("\x80");
        expect(!rejected.has_value() && rejected.error() == TextFailure::invalid_utf8,
               "the final continuation rejects overlong, surrogate and out-of-range scalars");
        expect(pending.finish() == Result{std::unexpected(TextFailure::invalid_utf8)},
               "rejected completion leaves the original pending sequence unfinished");
        const auto repeated = pending.with_bytes("\x80");
        expect(!repeated.has_value() && repeated.error() == TextFailure::invalid_utf8,
               "repeating completion observes the unchanged pending scalar bytes");
        expect(empty.finish() == Result{0},
               "pending and rejected branches preserve the empty base");
    }
}

void verify_pending_ownership()
{
    std::string source = "a\xE6\x97";
    const auto progress = Utf8Validation::empty().with_bytes(source).value();
    source.assign(source.size(), 'x');
    expect(progress.with_bytes("\xA5").value().finish() == Result{2},
           "the unfinished bytes do not alias the mutable input");
    const auto temporary = Utf8Validation::empty().with_bytes(std::string("\xF0\x9F\x99"));
    expect(temporary.value().with_bytes("\x82").value().finish() == Result{1},
           "the source can be destroyed between successive parts");
}

void verify_tail_then_next_sequence()
{
    const auto prefix = Utf8Validation::empty().with_bytes("\xC2").value();
    expect(prefix.with_bytes("\xA2"
                             "abc")
                   .value()
                   .finish() == Result{4},
           "bytes copied past a completed tail still reach the normal scanner");
    const auto japanese = Utf8Validation::empty().with_bytes("\xE6").value();
    const auto next = japanese.with_bytes("\x97\xA5\xF0\x9F\x99").value();
    expect(next.with_bytes("\x82").value().finish() == Result{2},
           "one part can finish the old sequence and leave a different unfinished sequence");
    const auto bad_after_tail = prefix.with_bytes("\xA2"
                                                  "a\xFF");
    expect(!bad_after_tail.has_value() && bad_after_tail.error() == TextFailure::invalid_utf8,
           "a malformed next character is not hidden by completing the old one");
}

void verify_one_byte_chunks()
{
    const std::string_view text("\0A\r\n日\xF0\x9F\x99\x82"
                                "Z",
                                12);
    auto progress = Utf8Validation::empty();
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const auto next = progress.with_bytes(text.substr(index, 1));
        expect(next.has_value(), "valid bytes can arrive one at a time including four-byte text");
        if (!next)
        {
            return;
        }
        progress = next.value().with_bytes("").value();
    }
    expect(progress.finish() == Result{7},
           "one-byte parts preserve NUL, CRLF and code point count");
}

void verify_ascii_boundaries()
{
    for (std::size_t padding = 0; padding <= 17; ++padding)
    {
        const std::string text =
            std::string(padding, 'a') + "日" + std::string(17, '\0') + "\xF0\x9F\x99\x82";
        expect_partitions(text, padding + 19);
    }
}
} // namespace

void verify_utf8_stream_contracts()
{
    verify_valid_partitions();
    verify_invalid_partitions();
    verify_empty_parts_and_finish();
    verify_branches_and_rejected_append();
    verify_scalar_rejection_timing();
    verify_pending_ownership();
    verify_tail_then_next_sequence();
    verify_one_byte_chunks();
    verify_ascii_boundaries();
}
} // namespace nenenib::tests
