#include "LineNumber.hpp"
#include "NewlineIndex.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "VirtualColumnIndex.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::core::LineNumber;
using nenenib::core::NewlineIndex;
using nenenib::core::Offset;
using nenenib::core::TextBuffer;
using nenenib::core::VirtualColumnIndex;

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

// 本文が持つ仮想桁の索引が、同じ本文から一から作った索引と同じ答えを返す（ADR 0140 の決定 4）。
// 比べるのは塊の境目の前後と一定の間隔の位置の桁・序数と、行ごとの逆引き。
[[nodiscard]] bool same_as_fresh(const TextBuffer &text)
{
    const auto &held = text.virtual_columns();
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
    expect(TextBuffer::empty().virtual_columns().chunk_count() == 0,
           "the empty text holds an empty column index");
    const auto text = buffer_of(mixed_columns_text());
    expect(text.virtual_columns().chunk_count() == (text.size_bytes() / column_chunk) + 1,
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
    expect(emptied.virtual_columns().chunk_count() == 0 && same_as_fresh(emptied),
           "an emptied text holds an empty column index");
    const auto refilled = emptied.insert(Offset{0}, "\tz\xe6\x97\xa5");
    expect(refilled.virtual_columns().virtual_column_at(refilled, Offset{5}) == 11 &&
               same_as_fresh(refilled),
           "typing into an empty text builds the held index");
    expect(same_as_fresh(text), "editing leaves the earlier text's index untouched");
}

// 中央への 1 文字の編集は、触れた塊の 1 つだけを作り直す。
void verify_held_columns_rebuild_one_chunk()
{
    const auto text = buffer_of(mixed_columns_text());
    const Offset middle{text.size_bytes() / 2};
    const auto typed = text.insert(middle, "x");
    expect(typed.virtual_columns().rebuilt_chunks() == 1,
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
    std::vector<TextBuffer> texts{buffer_of(mixed_columns_text())};
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
    const auto text = buffer_of(std::string(size, 'x'));
    const auto &index = text.virtual_columns();
    expect(index.chunk_count() == size / column_chunk,
           "a 16 MiB ASCII line holds one summary per 64 KiB");
    expect(std::ranges::all_of(index.chunks(), [](const auto &chunk) { return chunk.plain; }),
           "building a 16 MiB ASCII line walks no chunk");
    const auto end = index.cursor_at(text, Offset{size});
    expect(end.scanned == 0 && end.column == size && end.code_point == size,
           "the end of a 16 MiB ASCII line is read from the summaries");
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
}
} // namespace nenenib::tests
