#include "BytesReservation.hpp"
#include "DetectedText.hpp"
#include "Scopes.hpp"
#include "SealedBytes.hpp"
#include "TestSupport.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <optional>
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
using core::BytesReservation;
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

// 予約（ADR 0141 の追補 11 (e)）。複写できず、封印の前の列を読む口が無い。
template <typename Reservation>
concept ViewsReservation = requires(Reservation &reservation) { reservation.view(); };
template <typename Reservation>
concept ExposesReservationData = requires(Reservation &reservation) { reservation.data(); };
template <typename Reservation>
concept SealsReservationLvalue = requires(Reservation &reservation) { reservation.seal(); };

static_assert(!std::is_copy_constructible_v<BytesReservation>);
static_assert(!std::is_copy_assignable_v<BytesReservation>);
static_assert(std::is_nothrow_move_constructible_v<BytesReservation>);
static_assert(!std::is_default_constructible_v<BytesReservation>);
static_assert(!ViewsReservation<BytesReservation>);
static_assert(!ExposesReservationData<BytesReservation>);
static_assert(!SealsReservationLvalue<BytesReservation>);
static_assert(!std::is_constructible_v<SealedBytes, std::string, std::optional<std::size_t>>);

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
    const std::array<std::string, 12> samples{"",
                                              std::string("\xFF\xFE\x42\x30\x0D\x00\x0A\x00", 8),
                                              std::string("\xFE\xFF\x30\x42", 4),
                                              std::string("\xFF\xFE\x41", 3),
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

// BOM の本文は列を消さずに最初の piece の開始を 3 にずらす（ADR 0159 の決定 8）。BOM の無い同じ
// 本文と、読む口（全文・行・位置・最長の行・改行の形・仮想桁の索引）と編集の後の本文が一致し、
// piece は 1 本のまま（写していない）。
void expect_same_reading(const TextBuffer &shifted, const TextBuffer &plain, const char *message)
{
    bool same = shifted.text() == plain.text() && shifted.size_bytes() == plain.size_bytes() &&
                shifted.line_count() == plain.line_count() &&
                shifted.line_ending() == plain.line_ending() &&
                shifted.longest_line_bytes() == plain.longest_line_bytes() &&
                shifted.segments().size() == plain.segments().size();
    for (std::size_t line = 1; line <= plain.line_count() + 1; ++line)
    {
        const LineNumber number{line};
        same = same && shifted.line_start(number) == plain.line_start(number) &&
               shifted.line_end(number) == plain.line_end(number) &&
               shifted.line_terminator_end(number) == plain.line_terminator_end(number) &&
               shifted.line_text(number) == plain.line_text(number);
    }
    for (std::size_t at = 0; at <= plain.size_bytes(); ++at)
    {
        same = same && shifted.position_of(Offset{at}) == plain.position_of(Offset{at}) &&
               shifted.offset_of(plain.position_of(Offset{at})) ==
                   plain.offset_of(plain.position_of(Offset{at}));
    }
    expect(same, message);
}

// 1 つの本文の BOM のずらし。BOM の無い同じ本文と、そのまま・仮想桁の索引つき・先頭と末尾の編集の
// 後で読む口が一致する。
void verify_bom_shift_of(const std::string &sample)
{
    auto bom = DetectedText::from_bytes(std::string(byte_order_mark()) + sample).value();
    const auto shifted = TextBuffer::from_utf8(std::move(bom)).value();
    const auto plain = TextBuffer::from_utf8(DetectedText::from_bytes(sample).value()).value();
    expect(shifted.piece_count() == (sample.empty() ? 0U : 1U),
           "the shifted BOM body is one piece over the read bytes");
    expect(shifted.line_start(LineNumber{0}) == Offset{0} &&
               shifted.line_start(LineNumber{1}) == Offset{0},
           "line 1 starts at offset 0 after the BOM");
    expect_same_reading(shifted, plain, "a shifted BOM body reads like the plain body");
    expect_same_reading(shifted.with_virtual_columns(), plain.with_virtual_columns(),
                        "the virtual column index agrees after the BOM shift");
    expect_same_reading(shifted.insert(Offset{0}, "前\n"), plain.insert(Offset{0}, "前\n"),
                        "an insert at the top agrees after the BOM shift");
    const Offset cut{std::min<std::size_t>(2, sample.size())};
    expect_same_reading(shifted.erase(Offset{0}, cut), plain.erase(Offset{0}, cut),
                        "an erase at the top agrees after the BOM shift");
    expect_same_reading(shifted.insert(Offset{sample.size()}, "後"),
                        plain.insert(Offset{sample.size()}, "後"),
                        "an insert at the end agrees after the BOM shift");
}

void verify_bom_shift()
{
    const std::array<std::string, 6> samples{
        "", "x", "\n", "一行目\r\n二行目\r\n", "a\tb\nc\x01\n\n末尾", std::string("\r\n", 2)};
    for (const auto &sample : samples)
    {
        verify_bom_shift_of(sample);
    }
}

// UTF-16 は BOM を外して UTF-8 へ写した列を持ち、本文はその UTF-8 から作る（ADR 0149 の決定 2）。
void verify_detected_utf16()
{
    auto little = DetectedText::from_bytes(
                      std::string("\xFF\xFE\x42\x30\x0D\x00\x0A\x00\x3D\xD8\x00\xDE", 12))
                      .value();
    expect(little.encoding() == TextEncoding::utf16_le && little.bytes() == "あ\r\n😀",
           "UTF-16 LE is held as UTF-8 without its BOM");
    const auto text = TextBuffer::from_utf8(std::move(little)).value();
    expect(text.text() == "あ\r\n😀" && text.line_ending() == LineEnding::crlf &&
               text.line_count() == 2,
           "the newline form is decided on the UTF-8 body");
    auto big = DetectedText::from_bytes(std::string("\xFE\xFF\x00\x61\x00\x0A\x65\xE5", 8)).value();
    expect(big.encoding() == TextEncoding::utf16_be && big.bytes() == "a\n日",
           "UTF-16 BE is swapped before it becomes UTF-8");
    expect(TextBuffer::from_utf8(std::move(big)).value().line_ending() == LineEnding::lf,
           "an LF UTF-16 file keeps LF");
    auto empty = DetectedText::from_bytes("\xFF\xFE").value();
    expect(empty.bytes().empty() && TextBuffer::from_utf8(std::move(empty)).value().text().empty(),
           "a BOM-only UTF-16 file is an empty body");
    expect(DetectedText::from_bytes(std::string("\xFE\xFF\x00", 3)).error() ==
               EncodingFailure::odd_utf16_length,
           "an odd UTF-16 body is refused before conversion");
    expect(DetectedText::from_sealed(sealed_of(std::string("\xFF\xFE\x00\xDE", 4))).error() ==
               EncodingFailure::undecodable,
           "an unpaired surrogate in sealed bytes is undecodable");
}

// 予約を step バイトずつの窓で埋めて封印する（塊の切り方だけを変える）。
[[nodiscard]] SealedBytes sealed_in_steps(std::string_view source, std::size_t step)
{
    auto reservation = BytesReservation::reserve(source.size());
    std::size_t written = 0;
    while (written < source.size())
    {
        const auto target = reservation.window(step);
        std::copy_n(source.data() + written, target.size(), target.data());
        reservation.commit(target.size());
        written += target.size();
    }
    return std::move(reservation).seal();
}

// 追補 11 (f): 窓は宣言の残りを超えず、commit の超過は切り詰められ、封印の長さは確定の合計。
void verify_reservation_lengths()
{
    auto reservation = BytesReservation::reserve(10);
    const auto first = reservation.window(4);
    expect(first.size() == 4, "a window is as long as asked while the declaration has room");
    std::ranges::fill(first, 'a');
    reservation.commit(4);
    const auto second = reservation.window(100);
    expect(second.size() == 6, "a window never exceeds the declared remainder");
    std::ranges::fill(second.first(2), 'b');
    reservation.commit(2);
    const auto third = reservation.window(3);
    std::ranges::fill(third, 'c');
    reservation.commit(50);
    expect(reservation.window(100).size() == 1,
           "an excessive commit is cut to its window and the remainder stays");
    reservation.commit(0);
    const auto sealed = std::move(reservation).seal();
    expect(sealed.view() == "aaaabbccc" && sealed.size() == 9,
           "the sealed length is the sum of the committed lengths");
    auto none = BytesReservation::reserve(0);
    expect(none.window(8).empty(), "an empty declaration lends an empty window");
    expect(std::move(none).seal().size() == 0, "an empty declaration seals empty bytes");
}

// 追補 11 (g): 塊の切り方を変えても、判定と code point の数は借りた列の判定と同じ。
void verify_reservation_chunking()
{
    const std::array<std::string, 9> samples{"",
                                             "ASCII\n",
                                             "日本語\r\n続き😀",
                                             std::string(byte_order_mark()) + "あいう",
                                             "\x93\xFA\x96\x7B",
                                             "\xFF",
                                             std::string("\xFF\xFE\x42\x30", 4),
                                             "\xE3\x81",
                                             std::string(70000, 'a') + "あ" +
                                                 std::string(70000, 'b')};
    const std::array<std::size_t, 4> steps{1, 2, 4, 65536};
    for (const auto &sample : samples)
    {
        const auto borrowed = DetectedText::from_bytes(sample);
        const auto counted = core::validate_utf8(sample);
        const std::optional<std::size_t> points =
            counted.has_value() ? std::optional<std::size_t>(counted.value()) : std::nullopt;
        for (const std::size_t step : steps)
        {
            auto sealed = sealed_in_steps(sample, step);
            expect(sealed.view() == sample, "every chunking seals the same bytes");
            expect(sealed.utf8_code_points() == points,
                   "every chunking counts the same code points as one validation");
            const auto detected = DetectedText::from_sealed(std::move(sealed));
            expect(detected.has_value() == borrowed.has_value() &&
                       (!borrowed.has_value() ||
                        (detected.value().encoding() == borrowed.value().encoding() &&
                         detected.value().bytes() == borrowed.value().bytes())) &&
                       (borrowed.has_value() || detected.error() == borrowed.error()),
                   "every chunking detects like the borrowed bytes");
        }
    }
    // 3 バイトの文字の途中で窓を切る（あ = E3 81 82）。
    auto split = BytesReservation::reserve(4);
    const std::string_view kana = "xあ";
    auto head = split.window(2);
    std::copy_n(kana.data(), 2, head.data());
    split.commit(2);
    auto tail = split.window(2);
    std::copy_n(kana.data() + 2, 2, tail.data());
    split.commit(2);
    const auto joined = std::move(split).seal();
    expect(joined.utf8_code_points() == std::optional<std::size_t>(2),
           "a character split across windows is validated whole");
}

// 追補 11 (h): filled は reserve → seal と同じ列と検証を作る。
void verify_filled_is_reservation()
{
    const std::array<std::string, 4> samples{"", "abc", "日本", "\x93\xFA"};
    for (const auto &sample : samples)
    {
        const auto filled = sealed_of(sample);
        const auto reserved = sealed_in_steps(sample, sample.size() + 1);
        expect(filled.view() == reserved.view() &&
                   filled.utf8_code_points() == reserved.utf8_code_points(),
               "filled seals the same bytes and validation as a reservation");
    }
}
} // namespace

void verify_detected_text_contracts()
{
    verify_detected_utf16();
    verify_detected_rejections();
    verify_detected_boundaries();
    verify_bom_shift();
    verify_owned_and_borrowed();
    verify_defensive_boundaries();
    verify_sealed_fill();
    verify_sealed_detection();
    verify_reservation_lengths();
    verify_reservation_chunking();
    verify_filled_is_reservation();
}
} // namespace nenenib::tests
