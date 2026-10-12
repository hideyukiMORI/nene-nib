#include "LineNumber.hpp"
#include "NewlineIndex.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"

#include <array>
#include <limits>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using nenenib::core::LineNumber;
using nenenib::core::NewlineIndex;
using nenenib::core::Offset;
using nenenib::core::TextBuffer;

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
}
} // namespace nenenib::tests
