#include "TextBuffer.hpp"

#include "Utf8.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr char newline = '\n';
constexpr char carriage_return = '\r';

[[nodiscard]] const std::string &buffer_of(PieceSource source, const std::string &original,
                                           const std::string &add) noexcept
{
    switch (source)
    {
    case PieceSource::original:
        return original;
    case PieceSource::add:
        return add;
    }
    std::unreachable();
}

// 行索引の素。'\n' のバイト位置だけを持つ（CRLF は 1 つの改行なので数が狂わない・ADR 0009）。
[[nodiscard]] std::vector<Offset> newlines_in(std::string_view text)
{
    std::vector<Offset> found;
    // 走査は自前で書く。std::string_view::find は MSVC STL の __std_find_trivial_1 を残し、
    // それが core の許可シンボル（eng/symbol-allowlist.json）に無いので ARC-003 が落ちる。
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (text[index] == newline)
        {
            found.push_back(Offset{index});
        }
    }
    return found;
}

[[nodiscard]] std::size_t counted_below(const std::vector<Offset> &newlines,
                                        std::size_t limit) noexcept
{
    const auto bound =
        std::ranges::lower_bound(newlines, limit, {}, [](Offset entry) { return entry.value; });
    return static_cast<std::size_t>(bound - newlines.begin());
}

// 末尾の add piece に続く入力は piece を増やさずに伸ばす。連続した入力で列が膨らまない。
void append_insertion(std::vector<Piece> &out, std::size_t start, std::string_view text)
{
    auto found = newlines_in(text);
    const bool continues = !out.empty() && out.back().source == PieceSource::add &&
                           out.back().start.value + out.back().length == start;
    if (!continues)
    {
        out.push_back(Piece{PieceSource::add, Offset{start}, text.size(), std::move(found)});
        return;
    }
    Piece &last = out.back();
    for (const Offset entry : found)
    {
        last.newlines.push_back(Offset{entry.value + last.length});
    }
    last.length += text.size();
}

[[nodiscard]] std::size_t total_length(const std::vector<Piece> &pieces) noexcept
{
    std::size_t bytes = 0;
    for (const auto &piece : pieces)
    {
        bytes += piece.length;
    }
    return bytes;
}

[[nodiscard]] std::size_t total_newlines(const std::vector<Piece> &pieces) noexcept
{
    std::size_t count = 0;
    for (const auto &piece : pieces)
    {
        count += piece.newlines.size();
    }
    return count;
}
} // namespace

TextBuffer::TextBuffer(Buffer original, Buffer add, std::vector<Piece> pieces)
    : original_(std::move(original)), add_(std::move(add)), pieces_(std::move(pieces)),
      size_bytes_(total_length(pieces_)), newline_count_(total_newlines(pieces_))
{
}

TextBuffer TextBuffer::empty()
{
    auto nothing = std::make_shared<const std::string>();
    return TextBuffer(nothing, nothing, std::vector<Piece>{});
}

std::expected<TextBuffer, TextFailure> TextBuffer::from_utf8(std::string_view text)
{
    const auto validated = validate_utf8(text);
    if (!validated)
    {
        return std::unexpected(validated.error());
    }
    auto original = std::make_shared<const std::string>(text);
    std::vector<Piece> pieces;
    if (!text.empty())
    {
        pieces.push_back(Piece{PieceSource::original, Offset{0}, text.size(), newlines_in(text)});
    }
    return TextBuffer(original, std::make_shared<const std::string>(), std::move(pieces));
}

std::string_view TextBuffer::view_of(const Piece &piece) const noexcept
{
    const std::string &buffer = buffer_of(piece.source, *original_, *add_);
    return std::string_view(buffer).substr(piece.start.value, piece.length);
}

Piece TextBuffer::clipped(const Piece &piece, std::size_t from, std::size_t length) const
{
    if (from == 0 && length == piece.length)
    {
        return piece;
    }
    const auto part = view_of(piece).substr(from, length);
    return Piece{piece.source, Offset{piece.start.value + from}, length, newlines_in(part)};
}

void TextBuffer::collect(std::vector<Piece> &out, std::size_t from, std::size_t to) const
{
    std::size_t absolute = 0;
    for (const auto &piece : pieces_)
    {
        const std::size_t begin = std::max(absolute, from);
        const std::size_t end = std::min(absolute + piece.length, to);
        if (begin < end)
        {
            out.push_back(clipped(piece, begin - absolute, end - begin));
        }
        absolute += piece.length;
    }
}

TextBuffer TextBuffer::replaced(Offset begin, Offset end, std::string_view text) const
{
    auto add = add_;
    std::vector<Piece> next;
    collect(next, 0, begin.value);
    if (!text.empty())
    {
        const std::size_t start = add->size();
        add = std::make_shared<const std::string>(*add + std::string(text));
        append_insertion(next, start, text);
    }
    collect(next, end.value, size_bytes_);
    return TextBuffer(original_, std::move(add), std::move(next));
}

TextBuffer TextBuffer::insert(Offset at, std::string_view text) const
{
    const Offset clamped{std::min(at.value, size_bytes_)};
    return replaced(clamped, clamped, text);
}

TextBuffer TextBuffer::erase(Offset begin, Offset end) const
{
    const std::size_t from = std::min(begin.value, size_bytes_);
    const std::size_t to = std::clamp(end.value, from, size_bytes_);
    return replaced(Offset{from}, Offset{to}, std::string_view{});
}

std::size_t TextBuffer::size_bytes() const noexcept
{
    return size_bytes_;
}

std::size_t TextBuffer::line_count() const noexcept
{
    return newline_count_ + 1;
}

std::size_t TextBuffer::piece_count() const noexcept
{
    return pieces_.size();
}

std::string TextBuffer::text_range(Offset begin, Offset end) const
{
    std::string result;
    std::size_t absolute = 0;
    for (const auto &piece : pieces_)
    {
        const std::size_t from = std::max(absolute, begin.value);
        const std::size_t to = std::min(absolute + piece.length, end.value);
        if (from < to)
        {
            result.append(view_of(piece).substr(from - absolute, to - from));
        }
        absolute += piece.length;
    }
    return result;
}

std::string TextBuffer::text() const
{
    return text_range(Offset{0}, Offset{size_bytes_});
}

std::size_t TextBuffer::newline_offset(std::size_t index) const noexcept
{
    std::size_t absolute = 0;
    std::size_t seen = 0;
    for (const auto &piece : pieces_)
    {
        if (seen + piece.newlines.size() > index)
        {
            return absolute + piece.newlines.at(index - seen).value;
        }
        seen += piece.newlines.size();
        absolute += piece.length;
    }
    return size_bytes_;
}

std::size_t TextBuffer::newlines_before(std::size_t at) const noexcept
{
    std::size_t absolute = 0;
    std::size_t count = 0;
    for (const auto &piece : pieces_)
    {
        if (absolute + piece.length >= at)
        {
            return count + counted_below(piece.newlines, at - absolute);
        }
        count += piece.newlines.size();
        absolute += piece.length;
    }
    return count;
}

Offset TextBuffer::line_start(LineNumber line) const noexcept
{
    if (line.value <= 1)
    {
        return Offset{0};
    }
    return Offset{std::min(newline_offset(line.value - 2) + 1, size_bytes_)};
}

Offset TextBuffer::line_terminator_end(LineNumber line) const noexcept
{
    if (line.value > newline_count_)
    {
        return Offset{size_bytes_};
    }
    return Offset{newline_offset(line.value - 1) + 1};
}

Offset TextBuffer::line_end(LineNumber line) const noexcept
{
    const std::size_t start = line_start(line).value;
    std::size_t stop = line.value > newline_count_ ? size_bytes_ : newline_offset(line.value - 1);
    // CRLF の '\r' は行の内容ではない。表示もキャレットもここで止める（ADR 0009 の決定 8）。
    if (stop > start && text_range(Offset{stop - 1}, Offset{stop}).front() == carriage_return)
    {
        --stop;
    }
    return Offset{stop};
}

std::string TextBuffer::line_text(LineNumber line) const
{
    return text_range(line_start(line), line_end(line));
}

TextPosition TextBuffer::position_of(Offset at) const
{
    const std::size_t clamped = std::min(at.value, size_bytes_);
    const LineNumber line{newlines_before(clamped) + 1};
    const std::string prefix = text_range(line_start(line), Offset{clamped});
    return TextPosition{line, Column{code_point_count(prefix) + 1}};
}

Offset TextBuffer::offset_of(const TextPosition &position) const
{
    const Offset start = line_start(position.line);
    const std::string content = text_range(start, line_end(position.line));
    std::size_t byte = 0;
    for (std::size_t step = 1; step < position.column.value && byte < content.size(); ++step)
    {
        byte = next_code_point(content, Offset{byte}).value;
    }
    return Offset{start.value + byte};
}
} // namespace nenenib::core
