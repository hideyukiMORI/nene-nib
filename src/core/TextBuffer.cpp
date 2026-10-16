#include "TextBuffer.hpp"

#include "AddChunk.hpp"
#include "Utf8.hpp"
#include "VirtualColumnIndex.hpp"
#include "VirtualColumnsFailure.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
constexpr char carriage_return = '\r';
// add の chunk の大きさ（ADR 0044 の決定 2 / 7）。これより大きい 1 回の挿入はその 1 本だけの chunk
// になる。
constexpr std::size_t chunk_bytes = std::size_t{64} * 1024U;

using Chunks = std::vector<std::shared_ptr<AddChunk>>;

// UTF-8 の継続 byte（10xxxxxx）でない byte はコードポイントの先頭。
[[nodiscard]] constexpr bool starts_code_point(char byte) noexcept
{
    constexpr unsigned char mask = 0xC0U;
    constexpr unsigned char continuation = 0x80U;
    return (static_cast<unsigned char>(byte) & mask) != continuation;
}

// 本文の半開範囲と各 piece の非空の交差だけを、その呼び出し中に読む（Issue #338）。
// offset / length は piece 内の位置。借用を保存せず、本文外の end で容量も取らない。
template <typename Visitor>
void visit_text_range(std::span<const Piece> pieces, Offset begin, Offset end, Visitor &&visitor)
{
    if (begin.value >= end.value)
    {
        return;
    }
    std::size_t absolute = 0;
    for (const auto &piece : pieces)
    {
        if (absolute >= end.value)
        {
            break;
        }
        const std::size_t from = std::max(absolute, begin.value);
        const std::size_t to = std::min(absolute + piece.length, end.value);
        if (from < to && !visitor(piece, from - absolute, to - from))
        {
            return;
        }
        absolute += piece.length;
    }
}

// 末尾の chunk をその場で伸ばせるか（ADR 0044 の決定 2）。bytes.size() が自分の知る長さと違えば
// 別の値が先に伸ばしたしるしで、上書きせず新しい chunk を始める。確保した容量は超えない。
[[nodiscard]] bool extends_in_place(const Chunks &chunks, std::size_t fill,
                                    std::size_t length) noexcept
{
    return !chunks.empty() && chunks.back()->bytes.size() == fill && fill <= chunk_bytes &&
           length <= chunk_bytes - fill;
}

// 末尾の add piece に続く入力は piece を増やさずに伸ばす。連続した入力で列が膨らまない。
// 続くのは同じ chunk の中だけで、chunk が変われば新しい piece になる（ADR 0044 の決定 3）。
void append_insertion(std::vector<Piece> &out, Piece piece)
{
    const bool continues = !out.empty() && out.back().source == PieceSource::add &&
                           out.back().chunk == piece.chunk &&
                           out.back().start.value + out.back().length == piece.start.value;
    if (!continues)
    {
        out.push_back(piece);
        return;
    }
    // 同じ chunk の索引で窓が隣り合うので、窓の端を進めるだけで済む（ADR 0047 の決定 4）。
    Piece &last = out.back();
    last.length += piece.length;
    last.newline_end = piece.newline_end;
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
        count += piece.newline_end - piece.newline_begin;
    }
    return count;
}
} // namespace

// 入力を add に書き、それを指す piece を返す。add の複製は起きない（ADR 0044 の決定 2）。
Piece TextBuffer::appended(Chunks &chunks, std::size_t &fill, std::string_view text)
{
    if (!extends_in_place(chunks, fill, text.size()))
    {
        auto chunk = std::make_shared<AddChunk>();
        chunk->bytes.reserve(std::max(chunk_bytes, text.size()));
        chunks.push_back(std::move(chunk));
        fill = 0;
    }
    // 索引は bytes と同じ先端の条件でだけ伸びるので、いつも bytes の '\n' と一致する（ADR 0047 の
    // 決定 1）。
    AddChunk &chunk = *chunks.back();
    const std::size_t start = fill;
    const std::size_t first = chunk.newlines.size();
    chunk.bytes.append(text);
    chunk.newlines.append(text);
    fill += text.size();
    const std::size_t last = chunk.newlines.size();
    return Piece{PieceSource::add, chunks.size() - 1, Offset{start}, text.size(), first, last};
}

// add は空の列で始まり、最初の挿入が 1 本目の chunk を作る（ADR 0044）。
TextBuffer::TextBuffer(Buffer original, Index original_newlines, Columns columns,
                       std::vector<Piece> pieces, LineEnding ending)
    : original_(std::move(original)), original_newlines_(std::move(original_newlines)),
      columns_(std::move(columns)), column_rebuilds_(0), add_fill_(0), pieces_(std::move(pieces)),
      size_bytes_(total_length(pieces_)), newline_count_(total_newlines(pieces_)), ending_(ending)
{
}

TextBuffer TextBuffer::empty()
{
    return TextBuffer(std::make_shared<const std::string>(),
                      std::make_shared<const NewlineIndex>(NewlineIndex::empty()), nullptr,
                      std::vector<Piece>{}, LineEnding::crlf);
}

std::expected<TextBuffer, TextFailure> TextBuffer::from_utf8(std::string_view text)
{
    std::string owned(text);
    const auto validated = validate_utf8(owned);
    if (!validated)
    {
        return std::unexpected(validated.error());
    }
    return from_validated_utf8(std::move(owned));
}

std::expected<TextBuffer, TextFailure> TextBuffer::from_utf8(DetectedText text)
{
    switch (text.encoding_)
    {
    case TextEncoding::utf8:
        break;
    case TextEncoding::utf8_bom:
        text.bytes_.erase(0, byte_order_mark().size());
        break;
    case TextEncoding::shift_jis:
        return std::unexpected(TextFailure::invalid_utf8);
    }
    return from_validated_utf8(std::move(text.bytes_));
}

TextBuffer TextBuffer::from_validated_utf8(std::string text)
{
    auto original = std::make_shared<const std::string>(std::move(text));
    // original の索引はここでだけ作り、以後の値はこれを共有する（ADR 0047 / 0080）。
    auto newlines = std::make_shared<const NewlineIndex>(NewlineIndex::from_text(*original));
    std::vector<Piece> pieces;
    if (!original->empty())
    {
        pieces.push_back(
            Piece{PieceSource::original, 0, Offset{0}, original->size(), 0, newlines->size()});
    }
    // 改行の形はここで 1 度だけ判別する。以後は本文が持ち回り、開く経路は自分で判別しない
    // （ADR 0036 の決定 1 / ADR 0010 の決定 4）。
    const LineEnding ending = detect_line_ending(*original);
    // 仮想桁の索引は要る文書だけが with_virtual_columns で持つ（ADR 0140 の決定 4）。
    return TextBuffer(std::move(original), std::move(newlines), nullptr, std::move(pieces), ending);
}

std::string_view TextBuffer::view_of(const Piece &piece) const noexcept
{
    return std::string_view(buffer_of(piece)).substr(piece.start.value, piece.length);
}

// piece が指すバッファ。add は piece の chunk の 1 本で、piece は chunk を跨がない（ADR 0044 の決定
// 4）。
const std::string &TextBuffer::buffer_of(const Piece &piece) const noexcept
{
    switch (piece.source)
    {
    case PieceSource::original:
        return *original_;
    case PieceSource::add:
        return add_.at(piece.chunk)->bytes;
    }
    std::unreachable();
}

const NewlineIndex &TextBuffer::index_of(const Piece &piece) const noexcept
{
    switch (piece.source)
    {
    case PieceSource::original:
        return *original_newlines_;
    case PieceSource::add:
        return add_.at(piece.chunk)->newlines;
    }
    std::unreachable();
}

// 切った側の窓はsource全体のbit索引で求める。本文は読まない（ADR 0121）。
Piece TextBuffer::clipped(const Piece &piece, std::size_t from, std::size_t length) const
{
    if (from == 0 && length == piece.length)
    {
        return piece;
    }
    const auto &index = index_of(piece);
    const std::size_t begin = piece.start.value + from;
    return Piece{piece.source,
                 piece.chunk,
                 Offset{begin},
                 length,
                 index.count_before(begin),
                 index.count_before(begin + length)};
}

void TextBuffer::collect(std::vector<Piece> &out, std::size_t from, std::size_t to) const
{
    visit_text_range(pieces_, Offset{from}, Offset{to},
                     [this, &out](const Piece &piece, std::size_t offset, std::size_t length)
                     {
                         out.push_back(clipped(piece, offset, length));
                         return true;
                     });
}

TextBuffer TextBuffer::replaced(Offset begin, Offset end, std::string_view text) const
{
    // 範囲は本文の中へ丸める（insert / erase が丸めていたのと同じ規則・#191）。
    const std::size_t from = std::min(begin.value, size_bytes_);
    const std::size_t to = std::clamp(end.value, from, size_bytes_);
    auto add = add_;
    std::size_t fill = add_fill_;
    std::vector<Piece> next;
    collect(next, 0, from);
    if (!text.empty())
    {
        append_insertion(next, appended(add, fill, text));
    }
    collect(next, to, size_bytes_);
    // 編集はバイト列を変えても改行の形は変えない（ARC-009 / ADR 0036 の決定 1）。
    TextBuffer result(original_, original_newlines_, columns_, std::move(next), ending_);
    result.add_ = std::move(add);
    result.add_fill_ = fill;
    // 索引を持つ本文だけが、触れた塊を作り直して後ろの塊を要約から足す（ADR 0140 の決定 4）。
    if (columns_ != nullptr)
    {
        result.columns_ = std::make_shared<const VirtualColumnIndex>(
            columns_->replaced(result, Offset{from}, Offset{to}, text));
        result.column_rebuilds_ = result.columns_->rebuilt_chunks();
    }
    return result;
}

TextBuffer TextBuffer::with_virtual_columns() const
{
    TextBuffer result = *this;
    result.column_rebuilds_ = 0;
    if (columns_ == nullptr)
    {
        result.columns_ =
            std::make_shared<const VirtualColumnIndex>(VirtualColumnIndex::from_text(result));
        result.column_rebuilds_ = result.columns_->rebuilt_chunks();
    }
    return result;
}

bool TextBuffer::has_virtual_columns() const noexcept
{
    return columns_ != nullptr;
}

std::expected<std::reference_wrapper<const VirtualColumnIndex>, VirtualColumnsFailure>
TextBuffer::virtual_columns() const &
{
    if (columns_ == nullptr)
    {
        return std::unexpected(VirtualColumnsFailure::not_held);
    }
    return std::cref(*columns_);
}

std::size_t TextBuffer::rebuilt_column_chunks() const noexcept
{
    return column_rebuilds_;
}

TextBuffer TextBuffer::insert(Offset at, std::string_view text) const
{
    return replaced(at, at, text);
}

TextBuffer TextBuffer::erase(Offset begin, Offset end) const
{
    return replaced(begin, end, std::string_view{});
}

LineEnding TextBuffer::line_ending() const noexcept
{
    return ending_;
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

std::vector<std::string_view> TextBuffer::segments() const &
{
    std::vector<std::string_view> result;
    result.reserve(pieces_.size());
    for (const auto &piece : pieces_)
    {
        result.push_back(view_of(piece));
    }
    return result;
}

std::string TextBuffer::text_range(Offset begin, Offset end) const
{
    std::string result;
    visit_text_range(pieces_, begin, end,
                     [this, &result](const Piece &piece, std::size_t offset, std::size_t length)
                     {
                         result.append(view_of(piece).substr(offset, length));
                         return true;
                     });
    return result;
}

char TextBuffer::byte_at(Offset at) const noexcept
{
    char result = '\0';
    bool found = false;
    visit_text_range(pieces_, at, Offset{at.value + 1},
                     [this, &result, &found](const Piece &piece, std::size_t offset, std::size_t)
                     {
                         result = view_of(piece)[offset];
                         found = true;
                         return true;
                     });
    // 唯一の caller は 0 <= stop-1 < size_bytes_ を満たす。範囲外の代替文字は返さない。
    if (!found)
    {
        std::unreachable();
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
        const std::size_t count = piece.newline_end - piece.newline_begin;
        if (seen + count > index)
        {
            const Offset entry = index_of(piece).at(piece.newline_begin + (index - seen));
            return absolute + (entry.value - piece.start.value);
        }
        seen += count;
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
            return count + index_of(piece).count_before(piece.start.value + (at - absolute)) -
                   piece.newline_begin;
        }
        count += piece.newline_end - piece.newline_begin;
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
    // LF の本文では '\r' は 1 文字なので外さない。この 1 つの分岐が改行の模型の正本で、
    // line_terminator_end / line_text / position_of / offset_of はこれに従う（ADR 0036 の決定 2）。
    if (ending_ == LineEnding::crlf && stop > start && byte_at(Offset{stop - 1}) == carriage_return)
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
    return position_read(at).first;
}

Offset TextBuffer::offset_of(const TextPosition &position) const
{
    return offset_read(position).first;
}

std::pair<TextPosition, std::size_t> TextBuffer::position_read(Offset at) const
{
    const std::size_t clamped = std::min(at.value, size_bytes_);
    const LineNumber line{newlines_before(clamped) + 1};
    const Offset start = line_start(line);
    // 索引の序数はコードポイントの先頭で数える。途中の位置は先頭の byte を数える歩きに任せる。
    if (columns_ != nullptr &&
        (clamped == size_bytes_ || starts_code_point(byte_at(Offset{clamped}))))
    {
        const auto cursor = columns_->cursor_at(*this, Offset{clamped});
        return {TextPosition{line, Column{cursor.code_point + 1}}, cursor.copied};
    }
    std::size_t code_points = 0;
    // 非継続 byte の個数は任意の byte 分割で加法的。途中 offset / piece も丸めない。
    visit_text_range(
        pieces_, start, Offset{clamped},
        [this, &code_points](const Piece &piece, std::size_t offset, std::size_t length)
        {
            code_points += code_point_count(view_of(piece).substr(offset, length));
            return true;
        });
    return {TextPosition{line, Column{code_points + 1}}, clamped - start.value};
}

std::pair<Offset, std::size_t> TextBuffer::offset_read(const TextPosition &position) const
{
    const Offset start = line_start(position.line);
    if (position.column.value <= 1)
    {
        return {start, 0};
    }
    if (columns_ != nullptr)
    {
        return columns_->reached_code_point(*this, position.line, position.column.value - 1);
    }
    std::size_t remaining = position.column.value - 1;
    std::size_t consumed = 0;
    visit_text_range(
        pieces_, start, line_end(position.line),
        [this, &remaining, &consumed](const Piece &piece, std::size_t offset, std::size_t length)
        {
            const auto slice = view_of(piece).substr(offset, length);
            std::size_t byte = 0;
            if (consumed != 0 && !is_boundary(slice, Offset{0}))
            {
                byte = next_code_point(slice, Offset{0}).value;
            }
            while (remaining > 0 && byte < slice.size())
            {
                byte = next_code_point(slice, Offset{byte}).value;
                --remaining;
            }
            consumed += byte;
            return remaining > 0 || byte == slice.size();
        });
    return {Offset{start.value + consumed}, consumed};
}
} // namespace nenenib::core
