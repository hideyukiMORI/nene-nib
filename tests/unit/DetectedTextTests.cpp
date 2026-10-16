#include "DetectedText.hpp"
#include "Scopes.hpp"
#include "SealedBytes.hpp"
#include "TestSupport.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nenenib::tests
{
namespace
{
using core::byte_order_mark;
using core::DetectedText;
using core::EncodingFailure;
using core::LineEnding;
using core::LineNumber;
using core::Offset;
using core::SealedBytes;
using core::TextBuffer;
using core::TextEncoding;
using core::TextFailure;

template <typename Text>
concept BorrowsDetectedBytes = requires(Text &&text) { std::forward<Text>(text).bytes(); };

static_assert(BorrowsDetectedBytes<const DetectedText &>);
static_assert(!BorrowsDetectedBytes<DetectedText>);
static_assert(!std::is_default_constructible_v<DetectedText>);
static_assert(!std::is_constructible_v<DetectedText, std::string, TextEncoding>);

// 封印済みの列（ADR 0141 決定 1・6 (a)）。複写できず、view() 以外に列へ届く公開の口が無い。
template <typename Bytes>
concept BorrowsSealedView = requires(Bytes &&bytes) { std::forward<Bytes>(bytes).view(); };
template <typename Bytes>
concept ExposesSealedData = requires(Bytes &bytes) { bytes.data(); };
template <typename Bytes>
concept ReleasesSealedBytes = requires(Bytes &&bytes) { std::forward<Bytes>(bytes).release(); };
template <typename Bytes>
concept WritesThroughSealedView = requires(Bytes &bytes) { bytes.view()[0] = 'x'; };

static_assert(!std::is_copy_constructible_v<SealedBytes>);
static_assert(!std::is_copy_assignable_v<SealedBytes>);
static_assert(std::is_nothrow_move_constructible_v<SealedBytes>);
static_assert(!std::is_default_constructible_v<SealedBytes>);
static_assert(!std::is_constructible_v<SealedBytes, std::string>);
static_assert(BorrowsSealedView<const SealedBytes &> && BorrowsSealedView<SealedBytes &>);
static_assert(!BorrowsSealedView<SealedBytes>);
static_assert(std::same_as<decltype(std::declval<const SealedBytes &>().view()), std::string_view>);
static_assert(!ExposesSealedData<SealedBytes>);
static_assert(!ReleasesSealedBytes<SealedBytes>);
static_assert(!WritesThroughSealedView<SealedBytes>);

[[nodiscard]] SealedBytes sealed_of(std::string_view source)
{
    return SealedBytes::filled(source.size(),
                               [source](std::span<char> target) noexcept
                               {
                                   std::copy_n(source.data(), target.size(), target.data());
                                   return target.size();
                               });
}

void verify_detected_rejections()
{
    expect(!DetectedText::from_bytes("\x93").has_value(), "a truncated CP932 pair is rejected");
    expect(DetectedText::from_bytes(std::string(byte_order_mark()) + "\x93\xFA").error() ==
               EncodingFailure::undecodable,
           "a UTF-8 BOM cannot label CP932 bytes");
    expect(!DetectedText::from_bytes(std::string(byte_order_mark()) + "\xFF").has_value(),
           "broken UTF-8 behind a BOM is rejected");
    auto japanese = DetectedText::from_bytes("\x93\xFA").value();
    expect(japanese.encoding() == TextEncoding::shift_jis && japanese.bytes() == "\x93\xFA",
           "CP932 is held with its own bytes and encoding");
    const auto buffer = TextBuffer::from_utf8(std::move(japanese));
    expect(!buffer.has_value() && buffer.error() == TextFailure::invalid_utf8,
           "CP932 cannot enter the UTF-8 buffer without the code page port");
}

void verify_owned_and_borrowed()
{
    std::string source(4096, 'a');
    source += "\n日本\r\n";
    const std::string_view borrowed(source);
    auto detected = DetectedText::from_bytes(borrowed).value();
    source.front() = 'b';
    expect(detected.bytes().front() == 'a', "a borrowed source is owned before detection");
    const auto copy = detected;
    const auto text = TextBuffer::from_utf8(std::move(detected)).value();
    const auto raw = TextBuffer::from_utf8(copy.bytes()).value();
    expect(text.text() == raw.text() && text.line_count() == raw.line_count(),
           "raw and detected owners share the same text construction");
    expect(copy.bytes().front() == 'a', "moving one owner preserves a copied owner");
    expect(text.line_ending() == LineEnding::lf && text.line_text(LineNumber{2}) == "日本\r",
           "the first LF keeps the later CR literal");
    const auto next = text.insert(Offset{0}, "\n").erase(Offset{1}, Offset{2});
    expect(text.line_count() == 3 && text.line_start(LineNumber{2}) == Offset{4097},
           "edits preserve the original owner's shared index");
    expect(next.line_count() == 4 && next.line_start(LineNumber{2}) == Offset{1},
           "a new add index and clipped original retain line positions");
}

void verify_defensive_boundaries()
{
    std::string source(4096, 'a');
    source += "\nvalid";
    char *const alias = source.data();
    auto detected = DetectedText::from_bytes(std::move(source)).value();
    alias[0] = '\xFF';
    alias[4096] = 'x';
    expect(detected.encoding() == TextEncoding::utf8 && detected.bytes().front() == 'a',
           "a source alias cannot invalidate the detected UTF-8 bytes");
    const auto text = TextBuffer::from_utf8(std::move(detected)).value();
    expect(text.text().front() == 'a' && text.line_count() == 2 &&
               text.line_start(LineNumber{2}) == Offset{4097},
           "a source alias cannot change the detected owner's newline index");
    std::string raw_source(4096, 'b');
    raw_source += "\nvalid";
    char *const raw_alias = raw_source.data();
    const auto raw = TextBuffer::from_utf8(std::move(raw_source)).value();
    raw_alias[0] = '\xFF';
    raw_alias[4096] = 'x';
    expect(raw.text().front() == 'b', "a source alias cannot invalidate a raw UTF-8 buffer");
    expect(raw.text().at(4096) == '\n' && raw.line_count() == 2 &&
               raw.line_start(LineNumber{2}) == Offset{4097},
           "a source alias cannot change the raw buffer's newline index");
}

void verify_sealed_fill()
{
    std::size_t offered = 0;
    const auto shorter = SealedBytes::filled(8,
                                             [&offered](std::span<char> target) noexcept
                                             {
                                                 offered = target.size();
                                                 std::ranges::fill(target.first(3), 'c');
                                                 return std::size_t{3};
                                             });
    expect(offered == 8, "the fill borrows a span of exactly the declared size");
    expect(shorter.view() == "ccc" && shorter.size() == 3,
           "a shorter fill seals only the length it wrote");
    const auto claimed = SealedBytes::filled(4,
                                             [](std::span<char> target) noexcept
                                             {
                                                 std::ranges::fill(target, 'z');
                                                 return target.size() + 10;
                                             });
    expect(claimed.view() == "zzzz", "a fill cannot seal more than the declared bytes");
    bool called = false;
    const auto empty = SealedBytes::filled(0,
                                           [&called](std::span<char>) noexcept
                                           {
                                               called = true;
                                               return std::size_t{0};
                                           });
    expect(!called && empty.size() == 0, "an empty declaration seals without calling the fill");
    auto source = sealed_of("moved");
    const auto moved = std::move(source);
    expect(moved.view() == "moved", "a sealed list moves without a copy constructor");
}

void verify_sealed_detection()
{
    const std::array<std::string, 9> samples{"",
                                             "ASCII",
                                             "日本語\r\n続き",
                                             std::string("a\0b\n", 4),
                                             std::string(byte_order_mark()) + "x",
                                             "\x93\xFA",
                                             "\x93",
                                             "\xFF",
                                             std::string(byte_order_mark()) + "\x93\xFA"};
    for (const auto &sample : samples)
    {
        const auto borrowed = DetectedText::from_bytes(sample);
        const auto sealed = DetectedText::from_sealed(sealed_of(sample));
        expect(borrowed.has_value() == sealed.has_value(),
               "sealed and borrowed bytes are accepted and rejected alike");
        expect(!borrowed.has_value() || !sealed.has_value() ||
                   (borrowed.value().encoding() == sealed.value().encoding() &&
                    borrowed.value().bytes() == sealed.value().bytes()),
               "sealed and borrowed bytes detect the same encoding and keep the same bytes");
        expect(borrowed.has_value() || sealed.has_value() || borrowed.error() == sealed.error(),
               "sealed and borrowed bytes fail for the same reason");
    }
    auto large = sealed_of(std::string(4096, 'a'));
    const char *const allocation = large.view().data();
    const auto detected = DetectedText::from_sealed(std::move(large)).value();
    expect(detected.bytes().data() == allocation,
           "the sealed allocation moves into the detected owner without a copy");
}

void verify_detected_boundaries()
{
    const std::array<std::string, 5> samples{"", "ASCII", "日本語\r\n続き",
                                             std::string("a\0b\n", 4), "\n\n"};
    for (const auto &sample : samples)
    {
        auto plain = DetectedText::from_bytes(sample).value();
        expect(plain.encoding() == TextEncoding::utf8 && plain.bytes() == sample,
               "UTF-8 wins and the exact bytes are owned");
        expect(TextBuffer::from_utf8(std::move(plain)).value().text() == sample,
               "plain empty, multibyte, NUL and adjacent newline bytes reach the buffer");
        auto bom = DetectedText::from_bytes(std::string(byte_order_mark()) + sample).value();
        expect(bom.encoding() == TextEncoding::utf8_bom, "a BOM is remembered by the owner");
        const auto text = TextBuffer::from_utf8(std::move(bom)).value();
        expect(text.text() == sample, "exactly the leading BOM is removed, including BOM-only");
    }
    auto doubled = DetectedText::from_bytes(std::string(byte_order_mark()) +
                                            std::string(byte_order_mark()) + "x")
                       .value();
    expect(TextBuffer::from_utf8(std::move(doubled)).value().text() ==
               std::string(byte_order_mark()) + "x",
           "a second BOM remains body text");
}
} // namespace

void verify_detected_text_contracts()
{
    verify_detected_rejections();
    verify_detected_boundaries();
    verify_owned_and_borrowed();
    verify_defensive_boundaries();
    verify_sealed_fill();
    verify_sealed_detection();
}
} // namespace nenenib::tests
