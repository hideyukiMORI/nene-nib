#include "Column.hpp"
#include "LineLengths.hpp"
#include "LineNumber.hpp"
#include "NewlineIndex.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "VirtualColumn.hpp"
#include "VirtualColumnIndex.hpp"
#include "VirtualColumnsFailure.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::core::Column;
using nenenib::core::LineLengths;
using nenenib::core::LineNumber;
using nenenib::core::NewlineIndex;
using nenenib::core::Offset;
using nenenib::core::TextBuffer;
using nenenib::core::TextPosition;
using nenenib::core::VirtualColumnIndex;
using nenenib::core::VirtualColumnsFailure;

constexpr std::size_t column_chunk = 64UL * 1024UL;

void verify_index_text(std::string_view text)
{
    const auto index = NewlineIndex::from_text(text);
    std::size_t count = 0;
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        expect(index.count_before(at) == count, "rank counts only preceding LF bytes");
        if (text.at(at) == '\n')
        {
            expect(index.at(count).value == at, "select returns the next LF byte position");
            ++count;
        }
    }
    expect(index.size() == count, "index size is the number of LF bytes");
    expect(index.count_before(text.size()) == count, "rank includes LF at the final byte");
    expect(index.count_before(std::numeric_limits<std::size_t>::max()) == count,
           "rank beyond EOF is clamped without overflow");
}

void verify_index_boundaries()
{
    constexpr std::array<std::size_t, 13> sizes{0,   1,   63,  64,   65,   511, 512,
                                                513, 575, 576, 1024, 1025, 1537};
    std::string sparse(1537, '\0');
    for (const auto at : {0U, 63U, 64U, 511U, 512U, 1023U, 1536U})
    {
        sparse.at(at) = '\n';
    }
    for (const auto size : sizes)
    {
        verify_index_text(std::string(size, 'x'));
        verify_index_text(std::string(size, '\n'));
        verify_index_text(std::string_view(sparse).substr(0, size));
    }
    verify_index_text("\r\n日\n本\r\n\r終");
    verify_index_text(std::string(1536, 'x') + "\n");
    const auto empty = NewlineIndex::empty();
    expect(empty.size() == 0 && empty.count_before(999) == 0, "empty index has no LF");
}

void verify_dense_index()
{
    constexpr std::size_t length = 1024U * 1024U;
    const auto index = NewlineIndex::from_text(std::string(length, '\n'));
    expect(index.size() == length, "one MiB of LF has one line break per byte");
    for (const auto at : {0U, 1U, 63U, 64U, 511U, 512U, 65535U, 65536U, 1048575U})
    {
        expect(index.at(at).value == at && index.count_before(at) == at,
               "dense large index preserves rank and select near word/block/chunk boundaries");
    }
}

// ASCII LF text makes the independent expected line number a direct character count.
void verify_buffer_index(const TextBuffer &buffer, std::string_view text)
{
    expect(buffer.text() == text, "index edits preserve the expected bytes");
    std::size_t line = 1;
    expect(buffer.line_start(LineNumber{line}).value == 0, "first line starts at zero");
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        if (text.at(at) == '\n')
        {
            expect(buffer.line_end(LineNumber{line}).value == at,
                   "edited line ends at the expected LF");
            expect(buffer.position_of(Offset{at}).line.value == line,
                   "LF itself belongs to the preceding line");
            ++line;
            expect(buffer.line_start(LineNumber{line}).value == at + 1,
                   "edited line starts immediately after LF");
            expect(buffer.position_of(Offset{at + 1}).line.value == line,
                   "position after LF belongs to the next line");
        }
    }
    expect(buffer.line_count() == line, "edited line count includes the last line");
    expect(buffer.line_end(LineNumber{line}).value == text.size(), "last line ends at EOF");
}

void verify_append_snapshots()
{
    const std::string prefix = std::string(62, 'a') + "\n";
    const auto first = TextBuffer::empty().insert(Offset{0}, prefix);
    const std::string extension = "\n" + std::string(447, 'b') + "\n\n\n";
    const auto second = first.insert(Offset{prefix.size()}, extension);
    const auto third = second.insert(Offset{second.size_bytes()}, "tail\n");
    const auto branch = first.insert(Offset{10}, "branch\n");
    verify_buffer_index(first, prefix);
    verify_buffer_index(second, prefix + extension);
    verify_buffer_index(third, prefix + extension + "tail\n");
    verify_buffer_index(branch, prefix.substr(0, 10) + "branch\n" + prefix.substr(10));
    const auto clipped = third.replaced(Offset{63}, Offset{513}, "replacement\n");
    const auto expected = prefix + extension + "tail\n";
    verify_buffer_index(clipped, expected.substr(0, 63) + "replacement\n" + expected.substr(513));
    verify_buffer_index(third, expected);
}

void verify_chunk_boundaries()
{
    const std::string text = std::string(511, 'x') + "\n" + std::string(65023, 'y') + "\n";
    const auto first = TextBuffer::empty().insert(Offset{0}, text);
    const auto second = first.insert(Offset{first.size_bytes()}, "after\n");
    const std::string large = "\n" + std::string(65536, 'z') + "\n";
    const auto third = second.insert(Offset{512}, large);
    verify_buffer_index(first, text);
    verify_buffer_index(second, text + "after\n");
    verify_buffer_index(third, text.substr(0, 512) + large + text.substr(512) + "after\n");
    verify_buffer_index(third.erase(Offset{512}, Offset{512 + large.size()}), text + "after\n");
}

// at から前へ進めた最初の code point の先頭（本文の末尾を越えない）。
[[nodiscard]] std::size_t code_point_start(const TextBuffer &text, std::size_t at)
{
    constexpr unsigned char mask = 0xC0;
    constexpr unsigned char continuation = 0x80;
    at = std::min(at, text.size_bytes());
    while (at < text.size_bytes())
    {
        const auto byte =
            static_cast<unsigned char>(text.text_range(Offset{at}, Offset{at + 1}).at(0));
        if ((byte & mask) != continuation)
        {
            break;
        }
        ++at;
    }
    return at;
}

// 索引の塊の境目の前後 3 バイト（code point の先頭へ寄せる前）。
void add_boundary_probes(std::vector<std::size_t> &probes, const VirtualColumnIndex &index)
{
    for (const auto &chunk : index.chunks())
    {
        for (std::size_t delta = 0; delta < 4; ++delta)
        {
            probes.push_back(chunk.start + delta);
            probes.push_back(chunk.start > delta ? chunk.start - delta : 0);
        }
    }
}

// 両方の索引の塊の境目の前後と、一定の間隔の位置。
[[nodiscard]] std::vector<std::size_t> column_probes(const TextBuffer &text,
                                                     const VirtualColumnIndex &held,
                                                     const VirtualColumnIndex &fresh)
{
    std::vector<std::size_t> probes{0, text.size_bytes()};
    add_boundary_probes(probes, held);
    add_boundary_probes(probes, fresh);
    for (std::size_t at = 0; at < text.size_bytes(); at += 997)
    {
        probes.push_back(at);
    }
    return probes;
}

// 本文が持つ仮想桁の索引（持つ本文だけに問う）。
[[nodiscard]] const VirtualColumnIndex &held_of(const TextBuffer &text)
{
    return text.virtual_columns().value().get();
}

// 索引を持つ本文。
[[nodiscard]] TextBuffer held_buffer_of(std::string_view text)
{
    return buffer_of(text).with_virtual_columns();
}

// 本文が持つ仮想桁の索引が、同じ本文から一から作った索引と同じ答えを返す（ADR 0140 の決定 4）。
// 比べるのは塊の境目の前後と一定の間隔の位置の桁・序数と、行ごとの逆引き。
[[nodiscard]] bool same_as_fresh(const TextBuffer &text)
{
    const auto &held = held_of(text);
    const auto fresh = VirtualColumnIndex::from_text(text);
    bool same = held.chunks().empty() == fresh.chunks().empty();
    for (const auto probe : column_probes(text, held, fresh))
    {
        const Offset at{code_point_start(text, probe)};
        const auto left = held.cursor_at(text, at);
        const auto right = fresh.cursor_at(text, at);
        same = same && left.column == right.column && left.code_point == right.code_point;
    }
    const auto step = std::max<std::size_t>(text.line_count() / 40, 1);
    for (std::size_t number = 1; number <= text.line_count(); number += step)
    {
        const LineNumber line{number};
        const auto width = fresh.virtual_column_at(text, text.line_end(line));
        for (const auto column :
             {std::size_t{0}, std::size_t{7}, std::size_t{8}, width / 2, width, width + 2})
        {
            same = same &&
                   held.byte_at_virtual_column(text, line, column) ==
                       fresh.byte_at_virtual_column(text, line, column) &&
                   held.byte_at_code_point(text, line, column) ==
                       fresh.byte_at_code_point(text, line, column);
        }
    }
    return same;
}

// 複数の塊にまたがる行（Tab・全角・結合文字）と、短い行の並び。
[[nodiscard]] std::string mixed_columns_text()
{
    std::string text;
    while (text.size() < (2 * column_chunk) + 300)
    {
        text.append("ab\tc\xe6\x97\xa5"
                    "d\xcc\x81 ");
    }
    text.append("\n");
    for (std::size_t count = 0; text.size() < (4 * column_chunk) + 700; ++count)
    {
        text.append(count % 3 == 0 ? std::string("\t\xe6\x97\xa5x\n")
                                   : "line\tend " + std::to_string(count) + "\n");
    }
    return text;
}

// 編集の後の本文の索引は、一から作った索引と等しい（塊の境目・Tab・全角・複数行・空の本文）。
void verify_held_columns_after_edits()
{
    expect(held_of(TextBuffer::empty().with_virtual_columns()).chunk_count() == 0,
           "the empty text holds an empty column index");
    const auto text = held_buffer_of(mixed_columns_text());
    expect(held_of(text).chunk_count() == (text.size_bytes() / column_chunk) + 1,
           "a read text holds one column summary per 64 KiB");
    expect(same_as_fresh(text), "a read text holds the same column index as a fresh one");
    const Offset boundary{column_chunk - 5};
    const auto across = text.replaced(boundary, Offset{boundary.value + 30}, "\t\xe6\x97\xa5");
    expect(same_as_fresh(across), "an edit across a chunk boundary keeps the held index exact");
    const Offset middle{(2 * column_chunk) + 10};
    const auto lines = across.replaced(middle, middle, "\n\n\t\xe6\x97\xa5");
    expect(same_as_fresh(lines), "new line breaks keep the held index exact");
    const std::string large((2 * column_chunk) + 7, 'q');
    const auto grown = lines.insert(Offset{100}, large);
    expect(same_as_fresh(grown), "a large insertion keeps the held index exact");
    const auto emptied = grown.erase(Offset{0}, Offset{grown.size_bytes()});
    expect(held_of(emptied).chunk_count() == 0 && same_as_fresh(emptied),
           "an emptied text holds an empty column index");
    const auto refilled = emptied.insert(Offset{0}, "\tz\xe6\x97\xa5");
    expect(held_of(refilled).virtual_column_at(refilled, Offset{5}) == 11 &&
               same_as_fresh(refilled),
           "typing into an empty text builds the held index");
    expect(same_as_fresh(text), "editing leaves the earlier text's index untouched");
}

// 中央への 1 文字の編集は、触れた塊の 1 つだけを作り直す。
void verify_held_columns_rebuild_one_chunk()
{
    const auto text = held_buffer_of(mixed_columns_text());
    const Offset middle{text.size_bytes() / 2};
    const auto typed = text.insert(middle, "x");
    expect(typed.rebuilt_column_chunks() == 1,
           "one character typed in the middle rebuilds one chunk");
    expect(same_as_fresh(typed), "the typed text keeps the held index exact");
}

// undo / redo は逆向きの replaced で（EditorController::undo_edit / redo_edit と同じ形）、
// その後の索引も一から作った索引と等しい。
void verify_held_columns_undo_redo()
{
    using Step = std::tuple<std::size_t, std::size_t, std::string_view>;
    const std::array<Step, 4> steps{{{column_chunk - 2, 6, "\t\xe6\x97\xa5"},
                                     {(3 * column_chunk) + 1, 0, "\n\t"},
                                     {50, 200, ""},
                                     {(2 * column_chunk) + 9, 1, "\xe6\x97\xa5\xe6\x97\xa5"}}};
    std::vector<TextBuffer> texts{held_buffer_of(mixed_columns_text())};
    std::vector<std::string> removed;
    for (const auto &[at, length, inserted] : steps)
    {
        const auto last = texts.back();
        removed.push_back(last.text_range(Offset{at}, Offset{at + length}));
        texts.push_back(last.replaced(Offset{at}, Offset{at + length}, inserted));
    }
    auto undone = texts.back();
    for (std::size_t index = steps.size(); index > 0; --index)
    {
        const auto &[at, length, inserted] = steps.at(index - 1);
        undone = undone.replaced(Offset{at}, Offset{at + inserted.size()}, removed.at(index - 1));
        expect(undone.text() == texts.at(index - 1).text() && same_as_fresh(undone),
               "undo keeps the held index exact");
    }
    auto redone = undone;
    for (const auto &[at, length, inserted] : steps)
    {
        redone = redone.replaced(Offset{at}, Offset{at + length}, inserted);
        expect(same_as_fresh(redone), "redo keeps the held index exact");
    }
    expect(redone.text() == texts.back().text(), "redo returns to the edited text");
}

// 16 MiB の ASCII の 1 行は全ての塊が plain で、作るときも引くときも塊を歩かない。
void verify_held_columns_giant_ascii_line()
{
    constexpr std::size_t size = 16UL * 1024UL * 1024UL;
    const auto text = held_buffer_of(std::string(size, 'x'));
    const auto &index = held_of(text);
    expect(index.chunk_count() == size / column_chunk,
           "a 16 MiB ASCII line holds one summary per 64 KiB");
    expect(std::ranges::all_of(index.chunks(), [](const auto &chunk) { return chunk.plain; }),
           "building a 16 MiB ASCII line walks no chunk");
    const auto end = index.cursor_at(text, Offset{size});
    expect(end.scanned == 0 && end.column == size && end.code_point == size,
           "the end of a 16 MiB ASCII line is read from the summaries");
}

// 索引は既定で持たない。持たない本文の編集は塊を作り直さず（費用 0）、持つ本文の編集の後も持つ。
void verify_columns_absent_by_default()
{
    expect(!TextBuffer::empty().has_virtual_columns(), "the empty text holds no column index");
    const auto text = buffer_of(mixed_columns_text());
    expect(!text.has_virtual_columns() && text.rebuilt_column_chunks() == 0,
           "a read text holds no column index");
    const auto asked = text.virtual_columns();
    expect(!asked.has_value() && asked.error() == VirtualColumnsFailure::not_held,
           "asking a text without the index is refused");
    const auto typed = text.insert(Offset{text.size_bytes() / 2}, "x\n\t");
    expect(!typed.has_virtual_columns() && typed.rebuilt_column_chunks() == 0,
           "editing a text without the index rebuilds no chunk");
    const auto held = text.with_virtual_columns();
    expect(held.has_virtual_columns() &&
               held.rebuilt_column_chunks() == held_of(held).chunk_count(),
           "with_virtual_columns builds every chunk once");
    expect(!text.has_virtual_columns(), "with_virtual_columns leaves the earlier text untouched");
    const auto kept = held.insert(Offset{3}, "y");
    const auto again = kept.with_virtual_columns();
    expect(kept.has_virtual_columns() && kept.rebuilt_column_chunks() == 1 &&
               again.rebuilt_column_chunks() == 0 && &held_of(again) == &held_of(kept),
           "edits keep the index and asking again shares it without rebuilding");
}

// 行頭から at の手前までの桁と序数を 1 バイトずつ歩いて数える（ASCII の本文・索引に依らない答え）。
[[nodiscard]] std::pair<std::size_t, std::size_t> walked_columns(const TextBuffer &text,
                                                                 std::size_t at)
{
    const auto line_begin = text.line_start(text.position_of(Offset{at}).line).value;
    std::size_t column = 0;
    for (const char byte : text.text_range(Offset{line_begin}, Offset{at}))
    {
        column += nenenib::core::column_advance(static_cast<unsigned char>(byte), column);
    }
    return {column, at - line_begin};
}

// ASCII の短い行の並び。
[[nodiscard]] std::string ascii_lines_text(std::size_t lines)
{
    std::string text;
    for (std::size_t count = 0; count < lines; ++count)
    {
        text.append("line ").append(std::to_string(count)).append(" plain ascii text\n");
    }
    return text;
}

// 改行を含む ASCII の塊は plain で、桁と序数は改行の索引の行頭から引き、塊を歩かない。
void verify_held_columns_ascii_lines()
{
    const auto text = held_buffer_of(ascii_lines_text(200000));
    const auto &index = held_of(text);
    expect(std::ranges::all_of(index.chunks(), [](const auto &chunk) { return chunk.plain; }),
           "every chunk of ASCII lines is plain");
    bool exact = true;
    std::size_t scanned = 0;
    for (std::size_t at = 0; at <= text.size_bytes(); at += 4093)
    {
        const auto cursor = index.cursor_at(text, Offset{at});
        const auto [column, ordinal] = walked_columns(text, at);
        scanned += cursor.scanned;
        exact = exact && cursor.column == column && cursor.code_point == ordinal &&
                index.virtual_column_at(text, Offset{at}) == column &&
                index.code_point_at(text, Offset{at}) == ordinal;
    }
    expect(exact, "ASCII lines answer the walked column and ordinal");
    expect(scanned == 0, "ASCII lines are answered without walking a chunk");
    const auto typed = text.insert(Offset{text.size_bytes() / 2}, "x");
    const Offset after{(text.size_bytes() / 2) + 1};
    const auto &typed_index = held_of(typed);
    expect(typed.rebuilt_column_chunks() == 1 && typed_index.cursor_at(typed, after).scanned == 0 &&
               typed_index.virtual_column_at(typed, after) ==
                   walked_columns(typed, after.value).first,
           "one character typed in ASCII lines rebuilds one chunk and walks none");
}

// Tab と制御文字（0x20 未満と 0x7F・改行を除く）を含む ASCII の塊は plain でない。
void verify_held_columns_ascii_controls()
{
    for (const std::string_view mark : {"\t", "\x01", "\x7f", "\r"})
    {
        auto source = ascii_lines_text(8000);
        source.insert((3 * column_chunk) / 2, mark);
        const auto text = held_buffer_of(source);
        const auto &index = held_of(text);
        const auto chunks = index.chunks();
        const auto &marked = chunks[1];
        expect(!marked.plain && chunks[0].plain && chunks[2].plain,
               "only the chunk with a tab or a control character is not plain");
        bool exact = true;
        for (std::size_t at = marked.start; at <= marked.start + marked.bytes; at += 61)
        {
            const auto [column, ordinal] = walked_columns(text, at);
            exact = exact && index.virtual_column_at(text, Offset{at}) == column &&
                    index.code_point_at(text, Offset{at}) == ordinal;
        }
        expect(exact, "a chunk with a tab or a control character answers the walked column");
    }
}

void verify_original_slices()
{
    const std::string text = "\n" + std::string(510, 'x') + "\n\n" + std::string(511, 'y') + "\n";
    const auto original = buffer_of(text);
    const auto edited = original.replaced(Offset{63}, Offset{513}, "new\n");
    const auto erased = edited.erase(Offset{0}, Offset{64});
    const auto expected = text.substr(0, 63) + "new\n" + text.substr(513);
    verify_buffer_index(original, text);
    verify_buffer_index(edited, expected);
    verify_buffer_index(erased, expected.substr(64));
}

// 索引の塊の大きさの上限（編集で切り直す前の 128 KiB）。O(塊) の読みはこれを越えない。
constexpr std::size_t column_chunk_limit = 2 * column_chunk;

// 全角の「あ」の数。16 MiB の 1 行。
constexpr std::size_t giant_wide_count = (16UL * 1024UL * 1024UL) / 3;

// 16 MiB の全角の 1 行（塊は plain でない）と、その後ろの短い行。
[[nodiscard]] std::string giant_wide_text()
{
    const std::string_view wide = "\xe3\x81\x82";
    std::string source;
    source.reserve((giant_wide_count * wide.size()) + 8);
    for (std::size_t index = 0; index < giant_wide_count; ++index)
    {
        source.append(wide);
    }
    source.append("\nend");
    return source;
}

// 索引を持つ 16 MiB の全角の 1 行で、行末の position_of と offset_of は塊の 1 つ以下しか読まない
// （ADR 0139 の決定 6）。
void verify_held_positions_giant_line()
{
    const auto text = held_buffer_of(giant_wide_text());
    const Offset end{giant_wide_count * 3};
    const auto [position, read] = text.position_read(end);
    expect(position == TextPosition{LineNumber{1}, Column{giant_wide_count + 1}},
           "the end of a held giant line is the walked position");
    expect(read <= column_chunk_limit,
           "position_of at the end of a held giant line reads one chunk");
    const auto [offset, reached] = text.offset_read(position);
    expect(offset == end, "offset_of returns to the end of a held giant line");
    expect(reached <= column_chunk_limit,
           "offset_of at the end of a held giant line reads one chunk");
    const auto [past, past_read] =
        text.offset_read(TextPosition{LineNumber{1}, Column{giant_wide_count + 9}});
    expect(past == end && past_read <= column_chunk_limit,
           "a column past a held giant line stops at its end from one chunk");
    expect(text.position_of(Offset{text.size_bytes()}) == TextPosition{LineNumber{2}, Column{4}},
           "the line after a held giant line keeps its position");
}

// walked と indexed が、塊の境目の前後（コードポイントの途中を含む）と一定の間隔の位置、
// 行ごとの桁（行末を越える桁を含む）で同じ答えを返す。
[[nodiscard]] bool same_positions(const TextBuffer &walked, const TextBuffer &indexed)
{
    bool same = true;
    for (const auto probe : column_probes(indexed, held_of(indexed), held_of(indexed)))
    {
        const Offset at{probe};
        const auto position = walked.position_of(at);
        same = same && indexed.position_of(at) == position &&
               indexed.offset_of(position) == walked.offset_of(position);
    }
    for (std::size_t number = 1; number <= walked.line_count(); number += 97)
    {
        const LineNumber line{number};
        const auto width = walked.position_of(walked.line_end(line)).column.value;
        for (const auto column : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{9},
                                  width / 2, width, width + 3})
        {
            const TextPosition position{line, Column{column}};
            same = same && indexed.offset_of(position) == walked.offset_of(position);
        }
    }
    return same;
}

// 索引を持つ本文と持たない本文で、position_of と offset_of の答えが同じ（LF / CRLF・Tab・
// 全角・結合文字・コードポイントの途中・行末を越える桁・編集の後）。
void verify_held_positions_match_walk()
{
    std::string crlf;
    for (const char byte : mixed_columns_text())
    {
        crlf.append(byte == '\n' ? std::string("\r\n") : std::string(1, byte));
    }
    for (const auto &source : {mixed_columns_text(), crlf})
    {
        const auto plain = buffer_of(source);
        const auto held = plain.with_virtual_columns();
        const Offset at{code_point_start(plain, column_chunk + 5)};
        expect(same_positions(plain, held),
               "a held text answers position_of and offset_of like the walk");
        expect(same_positions(plain.insert(at, "\xe6\x97\xa5\t\n"),
                              held.insert(at, "\xe6\x97\xa5\t\n")),
               "an edited held text answers position_of and offset_of like the walk");
    }
}

// 一から数えた最長の行のバイト数（'\n' を含まない）。
[[nodiscard]] std::size_t counted_longest(std::string_view text)
{
    std::size_t longest = 0;
    std::size_t current = 0;
    for (const char byte : text)
    {
        current = byte == '\n' ? 0 : current + 1;
        longest = std::max(longest, current);
    }
    return longest;
}

void expect_longest(const TextBuffer &buffer, const char *message)
{
    expect(buffer.longest_line_bytes() == counted_longest(buffer.text()), message);
}

// 塊の要約の畳み込みが一から数えた値と等しい（Issue #553）。空・改行だけ・塊の境目をまたぐ行・
// 最後の行・CRLF の '\r'。
void verify_longest_line_texts()
{
    expect(TextBuffer::empty().longest_line_bytes() == 0, "an empty text has no long line");
    const std::string spanning = "ab\n" + std::string(1500, 'x') + "\n" + std::string(30, 'y');
    const std::string crlf = "a\r\n" + std::string(700, 'z') + "\r\nb";
    for (const auto &text :
         {std::string(), std::string("\n\n\n"), std::string(511, 'x'), std::string(512, 'x'),
          std::string(511, 'x') + "\n", std::string(1025, 'x'), spanning, crlf,
          std::string(600, '\n') + std::string(2000, 'q') + "\n", std::string("a\nbb\nccc")})
    {
        expect_longest(buffer_of(text), "the folded longest line equals the counted one");
    }
}

// 編集の後も一から数えた値と等しい（境目をまたぐ挿入・最長の行を短くする削除・行を割る改行・
// add の chunk をまたぐ挿入・全部を消す）。
void verify_longest_line_edits()
{
    const std::string text = std::string(300, 'a') + "\n" + std::string(2000, 'b') + "\n" +
                             std::string(900, 'c') + "\n" + std::string(100, 'd');
    const auto original = buffer_of(text);
    expect_longest(original, "the opened text has the counted longest line");
    const auto shortened = original.erase(Offset{400}, Offset{1900});
    expect_longest(shortened, "erasing most of the longest line makes another line the longest");
    const auto split = original.insert(Offset{1200}, "\n");
    expect_longest(split, "a line break splits the longest line");
    const auto joined = original.erase(Offset{300}, Offset{301});
    expect_longest(joined, "joining two lines across a block boundary makes a longer line");
    const auto grown = original.insert(Offset{3205}, std::string(70'000, 'e'));
    expect_longest(grown, "a large insertion across add chunks extends the line");
    expect_longest(grown.insert(Offset{3210}, "\n"), "a break inside the inserted chunk counts");
    expect_longest(original.erase(Offset{0}, Offset{text.size()}), "erasing everything is empty");
    auto typed = original;
    for (std::size_t step = 0; step < 600; ++step)
    {
        typed = typed.insert(Offset{2100 + step}, step % 97 == 96 ? "\n" : "f");
    }
    expect_longest(typed, "typing many pieces keeps the counted longest line");
    std::size_t seed = 7;
    auto edited = original;
    for (std::size_t step = 0; step < 200; ++step)
    {
        seed = (seed * 1'103'515'245U + 12'345U) % 2'147'483'648U;
        const std::size_t at = seed % (edited.size_bytes() + 1);
        const std::size_t length = (seed / 7) % 700;
        edited =
            step % 3 == 0
                ? edited.erase(Offset{at}, Offset{at + length})
                : edited.insert(Offset{at}, std::string(length, 'g') + (step % 2 == 0 ? "\n" : ""));
        expect_longest(edited, "a sequence of edits keeps the counted longest line");
    }
}

// 64 KiB の塊の疎 / 密の表現（ADR 0121 の追補）。n 個の LF を 64 KiB に等間隔で置いた塊。
constexpr std::size_t index_chunk = 64UL * 1024UL;

[[nodiscard]] std::string chunk_with_newlines(std::size_t newlines, char filler)
{
    std::string chunk(index_chunk, filler);
    const std::size_t step = index_chunk / std::max<std::size_t>(newlines, 1);
    for (std::size_t ordinal = 0; ordinal < newlines; ++ordinal)
    {
        chunk.at((ordinal * step) + (step - 1)) = '\n';
    }
    return chunk;
}

// 一から数えた [begin, end) の行の長さの要約（LineLengths の定義どおり）。
[[nodiscard]] LineLengths counted_lengths(std::string_view text)
{
    LineLengths result{};
    std::size_t line_begin = 0;
    for (std::size_t at = text.find('\n'); at != std::string_view::npos;
         at = text.find('\n', at + 1))
    {
        const std::size_t length = at - line_begin;
        result.head = result.closed ? result.head : length;
        result.longest = result.closed ? std::max(result.longest, length) : 0;
        result.closed = true;
        line_begin = at + 1;
    }
    result.tail = text.size() - line_begin;
    result.head = result.closed ? result.head : result.tail;
    return result;
}

[[nodiscard]] bool same_lengths(const LineLengths &left, const LineLengths &right)
{
    return left.head == right.head && left.longest == right.longest && left.tail == right.tail &&
           left.closed == right.closed;
}

void expect_window(const NewlineIndex &index, std::string_view text, std::size_t begin,
                   std::size_t end)
{
    if (begin <= end && end <= text.size())
    {
        expect(same_lengths(index.lengths(begin, end),
                            counted_lengths(text.substr(begin, end - begin))),
               "the window summary equals the counted one at chunk edges");
    }
}

// 塊の境目の前後と塊の途中を両端にした窓の要約が、一から数えた要約と一致する。
void verify_index_lengths(std::string_view text)
{
    const auto index = NewlineIndex::from_text(text);
    const std::size_t size = text.size();
    const std::array<std::size_t, 5> edges{0, 1, std::min(index_chunk + 1, size), size / 2, size};
    for (const auto begin : edges)
    {
        for (const auto end : edges)
        {
            expect_window(index, text, begin, end);
        }
    }
    for (std::size_t boundary = index_chunk; boundary <= size; boundary += index_chunk)
    {
        expect_window(index, text, boundary - 1, std::min(boundary + 1, size));
        expect_window(index, text, boundary, size);
        expect_window(index, text, 1, boundary - 1);
    }
}

// 疎と密の塊が隣り合う本文・ちょうど 4,095 / 4,096 / 4,097 個の塊・64 KiB の境の LF・
// 200 KiB の 1 行・NUL と CRLF。rank / select を全位置で単純な走査と比べる。
void verify_sparse_dense_chunks()
{
    const std::string mixed = chunk_with_newlines(4095, 'a') + chunk_with_newlines(4096, '\0') +
                              chunk_with_newlines(4097, 'b') + chunk_with_newlines(3, 'c') +
                              std::string(index_chunk, '\n') + "tail";
    verify_index_text(mixed);
    verify_index_lengths(mixed);
    std::string edges(3 * index_chunk, 'x');
    for (const auto at : {index_chunk - 1, index_chunk, (2 * index_chunk) - 1, 2 * index_chunk})
    {
        edges.at(at) = '\n';
    }
    verify_index_text(edges);
    verify_index_lengths(edges);
    const std::string giant = "a\n" + std::string(200U * 1024U, 'g') + "\n" + "z";
    verify_index_text(giant);
    verify_index_lengths(giant);
    std::string crlf;
    for (std::size_t line = 0; line < 9000; ++line)
    {
        crlf += line % 3 == 0 ? std::string("\r\n") : std::string("ab\0\r\n", 5);
    }
    verify_index_text(crlf);
    verify_index_lengths(crlf);
}

// 全オフセットの行番号が、一から数えた「前の LF の数 + 1」と一致する（LF でない位置も見る）。
void verify_buffer_lines(const TextBuffer &buffer, std::string_view text)
{
    expect(buffer.text() == text, "the appended buffer holds the expected bytes");
    std::size_t line = 1;
    for (std::size_t at = 0; at <= text.size(); ++at)
    {
        expect(buffer.position_of(Offset{at}).line.value == line,
               "every offset belongs to the line counted from the LF before it");
        line += at < text.size() && text.at(at) == '\n' ? 1 : 0;
    }
}

// 密の書きかけの塊で、最後の LF の後ろに 512 byte 以上の LF でないバイトが続く（区切りの累積を
// 書いた所より後ろの区切りを問う）。2 回目以降の追記で密の塊をちょうど 64 KiB まで埋め切り、
// その次の挿入は新しい add の chunk へ行く（I-1 の (1) (2)・レビュー #810）。
void verify_dense_partial_appends()
{
    const std::string lines(5000, '\n');
    const std::string gap(2000, 'x');
    const auto dense = TextBuffer::empty().insert(Offset{0}, lines);
    const auto gapped = dense.insert(Offset{dense.size_bytes()}, gap);
    verify_buffer_lines(gapped, lines + gap);
    const auto resumed = gapped.insert(Offset{gapped.size_bytes()}, "\n" + gap + "\n");
    verify_buffer_lines(resumed, lines + gap + "\n" + gap + "\n");
    const std::string rest(index_chunk - resumed.size_bytes(), 'y');
    const auto filled = resumed.insert(Offset{resumed.size_bytes()}, rest);
    expect(filled.size_bytes() == index_chunk, "the add chunk is filled to exactly 64 KiB");
    const auto next = filled.insert(Offset{filled.size_bytes()}, "z\nz");
    const std::string all = lines + gap + "\n" + gap + "\n" + rest;
    verify_buffer_index(filled, all);
    verify_buffer_index(next, all + "z\nz");
    expect(next.position_of(Offset{all.size() - 1}).line.value == 5003 &&
               next.position_of(Offset{all.size() + 2}).line.value == 5004,
           "the filled dense chunk and the next add chunk keep the line numbers");
    verify_buffer_lines(gapped, lines + gap);
    const std::string opened = lines + gap;
    verify_index_text(opened);
    verify_index_text(chunk_with_newlines(5000, 'q') + opened);
}

// 一から数える要約に 1 byte を足す。current は最後の LF の後ろの byte 数。
void counted_byte(LineLengths &running, std::size_t &current, char byte)
{
    if (byte != '\n')
    {
        ++current;
        return;
    }
    if (running.closed)
    {
        running.longest = std::max(running.longest, current);
    }
    else
    {
        running.head = current;
        running.closed = true;
    }
    current = 0;
}

[[nodiscard]] LineLengths summary_of(const LineLengths &running, std::size_t current)
{
    if (!running.closed)
    {
        return LineLengths{current, 0, current, false};
    }
    return LineLengths{running.head, running.longest, current, true};
}

// 一から数えた窓の要約を、始まり begin から終わりへ 1 byte ずつ伸ばしながら作り、ends の各点で
// 索引の答えと比べる（各始まりで本文を 1 回だけ走査する）。
void expect_windows_from(const NewlineIndex &index, std::string_view text, std::size_t begin,
                         std::span<const std::size_t> ends)
{
    LineLengths running{};
    std::size_t current = 0;
    std::size_t at = begin;
    for (const auto end : ends)
    {
        for (; at < end; ++at)
        {
            counted_byte(running, current, text.at(at));
        }
        if (end >= begin)
        {
            expect(same_lengths(index.lengths(begin, end), summary_of(running, current)),
                   "every window over the chosen points equals the counted summary");
        }
    }
}

// 疎・密・疎の 3 塊（最後は短い）の本文で、塊の境・512 byte の区切り・LF の前後の点の組を総当たりで
// 比べる（I-1 の (3)・レビュー #810）。
void verify_lengths_exhaustive()
{
    const std::string text =
        chunk_with_newlines(40, 'a') + chunk_with_newlines(4100, 'b') + "c\nccc\n\ncc";
    const auto index = NewlineIndex::from_text(text);
    std::vector<std::size_t> points{0, 1, 2, 14, 15, 16, text.size() - 1, text.size()};
    for (const std::size_t base : {index_chunk, 2 * index_chunk})
    {
        for (std::size_t delta = 0; delta < 5; ++delta)
        {
            points.push_back(base + delta - 2);
        }
    }
    for (const std::size_t block : {1U, 2U, 63U, 127U})
    {
        points.insert(points.end(),
                      {index_chunk + (block * 512U) - 1U, index_chunk + (block * 512U),
                       index_chunk + (block * 512U) + 1U});
    }
    std::ranges::sort(points);
    const auto unique = std::ranges::unique(points);
    points.erase(unique.begin(), unique.end());
    for (const auto begin : points)
    {
        expect_windows_from(index, text, begin, points);
    }
}

// add の 1 本の chunk を打鍵で伸ばすと、索引の塊は疎のまま伸び、4,096 個目で密に変わり、その後は
// bit を立てるだけ。変わる前の値の行位置は変わった後も同じ（決定 6）。
void verify_typed_densify()
{
    auto typed = TextBuffer::empty().insert(Offset{0}, "x");
    std::string expected = "x";
    std::vector<std::pair<TextBuffer, std::string>> snapshots;
    for (std::size_t step = 0; step < 6000; ++step)
    {
        const std::string piece = step % 5 == 4 ? "ab\n" : "\n";
        typed = typed.insert(Offset{typed.size_bytes()}, piece);
        expected += piece;
        if (step == 3000 || step == 4094 || step == 4095 || step == 4200)
        {
            snapshots.emplace_back(typed, expected);
        }
    }
    verify_buffer_index(typed, expected);
    for (const auto &[snapshot, text] : snapshots)
    {
        verify_buffer_index(snapshot, text);
    }
    expect_longest(typed, "the typed chunk keeps the counted longest line after densifying");
}
} // namespace

void verify_newline_index_scope()
{
    verify_index_boundaries();
    verify_dense_index();
    verify_append_snapshots();
    verify_chunk_boundaries();
    verify_original_slices();
    verify_held_columns_after_edits();
    verify_held_columns_rebuild_one_chunk();
    verify_held_columns_undo_redo();
    verify_held_columns_giant_ascii_line();
    verify_columns_absent_by_default();
    verify_held_columns_ascii_lines();
    verify_held_columns_ascii_controls();
    verify_held_positions_giant_line();
    verify_held_positions_match_walk();
    verify_longest_line_texts();
    verify_longest_line_edits();
    verify_sparse_dense_chunks();
    verify_typed_densify();
    verify_dense_partial_appends();
    verify_lengths_exhaustive();
}
} // namespace nenenib::tests
