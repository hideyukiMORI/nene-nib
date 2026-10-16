#include "VirtualColumnIndex.hpp"

#include "Grid.hpp"
#include "OffsetRange.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
// 塊の大きさ（ADR 0140 の決定 4）。作るときは 64 KiB で切り、編集で伸びた範囲は 2 倍を越えたら
// 2 倍以下の等しい長さに切り直す。4 分の 1 を下回る範囲は次の塊と合わせて切り直す。
constexpr std::size_t chunk_bytes = 64UL * 1024UL;
constexpr std::size_t longest_chunk = 2 * chunk_bytes;
constexpr std::size_t merge_bytes = chunk_bytes / 4;
// 切り目を code point の先頭へ寄せるために余分に読むバイト数（UTF-8 の最長 4 バイト）。
constexpr std::size_t sequence_slack = 3;
constexpr char32_t tab_character = U'\t';
constexpr unsigned char first_plain = 0x20;
constexpr unsigned char last_plain = 0x7E;
constexpr unsigned char continuation_mask = 0xC0;
constexpr unsigned char continuation_bits = 0x80;

using Measure = std::size_t VirtualColumnCursor::*;

[[nodiscard]] constexpr VirtualColumnCursor zero_cursor() noexcept
{
    return VirtualColumnCursor{0, 0, 0, 0, 0};
}

// 塊を 1 度だけ読み、表示できる ASCII と '\n' だけか（plain）を返す。'\n' の数と最後の '\n' の
// 位置（無ければ npos）も数える。Tab と制御文字（'\n' 以外の 0x20 未満と 0x7F）と非 ASCII は
// 1 バイト＝1 桁でないので plain にしない。STL の数えと後ろからの検索は core の外へ別のシンボルを
// 出すので、ここで 1 バイトずつ数える（ARC-003）。
[[nodiscard]] bool classified(std::string_view bytes, std::size_t &breaks,
                              std::size_t &last) noexcept
{
    bool plain = true;
    breaks = 0;
    last = std::string_view::npos;
    for (std::size_t at = 0; at < bytes.size(); ++at)
    {
        const auto byte = static_cast<unsigned char>(bytes[at]);
        if (byte == '\n')
        {
            ++breaks;
            last = at;
            continue;
        }
        plain = plain && byte >= first_plain && byte <= last_plain;
    }
    return plain;
}

[[nodiscard]] bool continues(char value) noexcept
{
    return (static_cast<unsigned char>(value) & continuation_mask) == continuation_bits;
}

// column の次の Tab の止まり。止まりの値は column_advance の 1 本から読む（ADR 0034）。
[[nodiscard]] std::size_t next_stop(std::size_t column) noexcept
{
    return column + column_advance(tab_character, column);
}

[[nodiscard]] std::ptrdiff_t signed_of(std::size_t value) noexcept
{
    return static_cast<std::ptrdiff_t>(value);
}

// target バイト目から前へ進めた最初の code point の先頭（bytes の長さを越えない）。
[[nodiscard]] std::size_t cut_point(std::string_view bytes, std::size_t target) noexcept
{
    auto at = std::min(target, bytes.size());
    while (at < bytes.size() && continues(bytes[at]))
    {
        ++at;
    }
    return at;
}

// 1 文字ぶん state を進め、その文字が始まる升を返す（送りの規則は格子の 1 本・Grid.hpp）。
std::size_t stepped(VirtualColumnCursor &state, char32_t value, std::size_t columns) noexcept
{
    const auto advance = column_advance(value, state.column);
    auto start = state.cell;
    if (value != tab_character && columns != 0)
    {
        start = grid_placed_cell(state.cell, advance, columns);
    }
    state.column += advance;
    state.cell = start + advance;
    ++state.code_point;
    return start;
}

// bytes を歩いて state を進め、2 升以上の Tab 以外の表示（送りの対象）があったかを返す。
bool walked(std::string_view bytes, VirtualColumnCursor &state, std::size_t columns) noexcept
{
    bool wide = false;
    for (std::size_t at = 0; at < bytes.size(); at = next_code_point(bytes, Offset{at}).value)
    {
        const char32_t value = code_point_at(bytes, Offset{at});
        wide = wide || (value != tab_character && column_advance(value, 0) > 1);
        static_cast<void>(stepped(state, value, columns));
    }
    return wide;
}

[[nodiscard]] VirtualColumnCursor start_of(const VirtualColumnChunk &chunk) noexcept
{
    return VirtualColumnCursor{chunk.column, chunk.cell, 0, chunk.code_point, 0};
}

// '\n' を含まず plain でもない塊の要約。最初の Tab の手前と後ろを分けて歩く。
void summarize_line_part(VirtualColumnChunk &chunk, std::string_view bytes, std::size_t columns)
{
    auto state = start_of(chunk);
    const auto first_tab = bytes.find('\t');
    chunk.wide = walked(bytes.substr(0, first_tab), state, columns);
    chunk.lead = state.column - chunk.column;
    if (first_tab != std::string_view::npos)
    {
        chunk.tab = true;
        static_cast<void>(stepped(state, tab_character, columns));
        const auto after_tab = state.column;
        chunk.wide = walked(bytes.substr(first_tab + 1), state, columns) || chunk.wide;
        chunk.rest = state.column - after_tab;
    }
    chunk.padding = (state.cell - chunk.cell) - (state.column - chunk.column);
    chunk.code_points = state.code_point - chunk.code_point;
}

// [start, start + bytes) を state から始まる塊として要約する。'\n' を含む塊は最後の '\n' の
// 後ろだけを歩く（そこより前の桁は次の塊に効かない）。plain の塊は歩かず長さから決める。
// line は呼ぶ側が付ける。
[[nodiscard]] VirtualColumnChunk summarized(std::string_view bytes, std::size_t start,
                                            VirtualColumnCursor state, std::size_t columns)
{
    VirtualColumnChunk chunk{};
    chunk.start = start;
    chunk.bytes = bytes.size();
    chunk.column = state.column;
    chunk.cell = state.cell;
    chunk.code_point = state.code_point;
    std::size_t last_newline = 0;
    chunk.plain = classified(bytes, chunk.line_breaks, last_newline);
    const auto tail_bytes = bytes.substr(chunk.line_breaks == 0 ? 0 : last_newline + 1);
    if (chunk.plain)
    {
        chunk.resets = chunk.line_breaks != 0;
        chunk.lead = tail_bytes.size();
        chunk.rest = chunk.resets ? tail_bytes.size() : 0;
        chunk.tail_cell = chunk.rest;
        chunk.code_points = tail_bytes.size();
        return chunk;
    }
    if (chunk.line_breaks == 0)
    {
        summarize_line_part(chunk, bytes, columns);
        return chunk;
    }
    auto tail = zero_cursor();
    static_cast<void>(walked(tail_bytes, tail, columns));
    chunk.resets = true;
    chunk.rest = tail.column;
    chunk.tail_cell = tail.cell;
    chunk.code_points = tail.code_point;
    return chunk;
}

// 塊の終わりの累積。塊を読まずに要約から足す（Tab の余りは最初の Tab で補正する）。
[[nodiscard]] VirtualColumnCursor end_of(const VirtualColumnChunk &chunk) noexcept
{
    if (chunk.resets)
    {
        return VirtualColumnCursor{chunk.rest, chunk.tail_cell, 0, chunk.code_points, 0};
    }
    const auto column =
        chunk.tab ? next_stop(chunk.column + chunk.lead) + chunk.rest : chunk.column + chunk.lead;
    return VirtualColumnCursor{column, chunk.cell + (column - chunk.column) + chunk.padding, 0,
                               chunk.code_point + chunk.code_points, 0};
}

// 始まりが変わった塊の送りの空きを読み直す必要があるか。空きは桁の 8 の余りと升の columns の
// 余りだけで決まり、送りの対象が無い塊では 0 のまま。
[[nodiscard]] bool needs_rescan(const VirtualColumnChunk &chunk, VirtualColumnCursor state,
                                std::size_t columns) noexcept
{
    if (columns == 0 || !chunk.wide || chunk.resets)
    {
        return false;
    }
    return state.column % vim_tab_stop != chunk.column % vim_tab_stop ||
           state.cell % columns != chunk.cell % columns;
}

// 塊の中で at より前の最後の行頭（無ければ塊の始まり）。塊の要約が持つ「前の '\n' の数」と
// 「塊の中の '\n' の数」の間を、改行の索引（ADR 0047）の行頭で二分探索する。
[[nodiscard]] std::size_t line_start_within(const TextBuffer &text, const VirtualColumnChunk &chunk,
                                            std::size_t at)
{
    std::size_t low = 0;
    std::size_t high = chunk.line_breaks;
    while (low < high)
    {
        const auto middle = low + ((high - low + 1) / 2);
        if (text.line_start(LineNumber{chunk.line + middle + 1}).value <= at)
        {
            low = middle;
        }
        else
        {
            high = middle - 1;
        }
    }
    return low == 0 ? chunk.start : text.line_start(LineNumber{chunk.line + low + 1}).value;
}

// 本文の [begin, end)。本文が 1 本の piece ならその借用を、そうでなければ copied へ写して返す。
[[nodiscard]] std::string_view window_of(const TextBuffer &text,
                                         std::span<const std::string_view> segments,
                                         OffsetRange range, std::string &copied)
{
    if (segments.size() == 1)
    {
        return segments.front().substr(range.begin.value, range.end.value - range.begin.value);
    }
    copied = text.text_range(range.begin, range.end);
    return copied;
}
} // namespace

VirtualColumnIndex VirtualColumnIndex::from_text(const TextBuffer &text)
{
    return built(text, 0);
}

VirtualColumnIndex VirtualColumnIndex::for_grid(const TextBuffer &text, std::size_t columns)
{
    return built(text, std::max<std::size_t>(columns, 1));
}

VirtualColumnIndex VirtualColumnIndex::built(const TextBuffer &text, std::size_t columns)
{
    VirtualColumnIndex index;
    index.columns_ = columns;
    index.chunks_.reserve((text.size_bytes() / chunk_bytes) + 1);
    // 読んだばかりの本文は 1 本の piece なので写さずに読む（ADR 0140 の決定 4）。
    const auto segments = text.segments();
    std::string copied;
    std::size_t start = 0;
    while (start < text.size_bytes())
    {
        const auto stop = std::min(start + chunk_bytes + sequence_slack, text.size_bytes());
        const auto bytes =
            window_of(text, segments, OffsetRange{Offset{start}, Offset{stop}}, copied);
        const auto cut = cut_point(bytes, chunk_bytes);
        auto chunk = summarized(bytes.substr(0, cut), start, index.end_state(), columns);
        chunk.line = index.end_line();
        index.chunks_.push_back(chunk);
        start += cut;
    }
    index.rebuilt_ = index.chunks_.size();
    return index;
}

VirtualColumnCursor VirtualColumnIndex::end_state() const noexcept
{
    return chunks_.empty() ? zero_cursor() : end_of(chunks_.back());
}

std::size_t VirtualColumnIndex::end_line() const noexcept
{
    return chunks_.empty() ? 0 : chunks_.back().line + chunks_.back().line_breaks;
}

void VirtualColumnIndex::append_cut(const TextBuffer &text, std::size_t begin, std::size_t end)
{
    const auto bytes = text.text_range(Offset{begin}, Offset{end});
    const auto pieces = (bytes.size() + longest_chunk - 1) / longest_chunk;
    const auto target = pieces == 0 ? 0 : (bytes.size() + pieces - 1) / pieces;
    const std::string_view view(bytes);
    std::size_t from = 0;
    while (from < view.size())
    {
        const auto cut = cut_point(view, from + target);
        auto chunk = summarized(view.substr(from, cut - from), begin + from, end_state(), columns_);
        chunk.line = end_line();
        chunks_.push_back(chunk);
        ++rebuilt_;
        from = cut;
    }
}

void VirtualColumnIndex::append_carried(const VirtualColumnIndex &old, std::size_t from,
                                        std::size_t shift, const TextBuffer &text)
{
    if (from >= old.chunks_.size())
    {
        return;
    }
    const auto old_line = old.chunks_.at(from).line;
    const auto new_line = end_line();
    auto state = end_state();
    bool settled = false;
    for (auto at = from; at < old.chunks_.size(); ++at)
    {
        auto chunk = old.chunks_.at(at);
        chunk.start += shift;
        chunk.line = chunk.line - old_line + new_line;
        // 始まりが前と同じなら、ここから後ろの累積は変わらない（位置と行の数をずらすだけ）。
        settled = settled || (state.column == chunk.column && state.cell == chunk.cell &&
                              state.code_point == chunk.code_point);
        if (!settled && needs_rescan(chunk, state, columns_))
        {
            const auto bytes =
                text.text_range(Offset{chunk.start}, Offset{chunk.start + chunk.bytes});
            const auto line = chunk.line;
            chunk = summarized(bytes, chunk.start, state, columns_);
            chunk.line = line;
            ++rebuilt_;
        }
        chunk.column = state.column;
        chunk.cell = state.cell;
        chunk.code_point = state.code_point;
        chunks_.push_back(chunk);
        state = end_of(chunk);
    }
}

VirtualColumnIndex VirtualColumnIndex::replaced(const TextBuffer &after, Offset begin, Offset end,
                                                std::string_view inserted) const
{
    if (chunks_.empty())
    {
        return built(after, columns_);
    }
    const auto old_size = size_bytes();
    const auto low = std::min(begin.value, old_size);
    const auto removed = std::clamp(end.value, low, old_size) - low;
    const auto first = chunk_of(low);
    auto last = removed > 0 ? chunk_of(low + removed - 1) : first;
    const auto span_begin = chunks_.at(first).start;
    auto old_end = chunks_.at(last).start + chunks_.at(last).bytes;
    if (old_end - span_begin - removed + inserted.size() < merge_bytes && last + 1 < chunks_.size())
    {
        ++last;
        old_end += chunks_.at(last).bytes;
    }
    const auto new_end = old_end - removed + inserted.size();
    VirtualColumnIndex result;
    result.columns_ = columns_;
    result.chunks_.reserve(chunks_.size() + (inserted.size() / chunk_bytes) + 1);
    result.chunks_.assign(chunks_.begin(), std::next(chunks_.begin(), signed_of(first)));
    result.append_cut(after, span_begin, new_end);
    result.append_carried(*this, last + 1, new_end - old_end, after);
    return result;
}

std::size_t VirtualColumnIndex::columns() const noexcept
{
    return columns_;
}

std::size_t VirtualColumnIndex::chunk_count() const noexcept
{
    return chunks_.size();
}

std::span<const VirtualColumnChunk> VirtualColumnIndex::chunks() const noexcept
{
    return chunks_;
}

std::size_t VirtualColumnIndex::rebuilt_chunks() const noexcept
{
    return rebuilt_;
}

std::size_t VirtualColumnIndex::size_bytes() const noexcept
{
    return chunks_.empty() ? 0 : chunks_.back().start + chunks_.back().bytes;
}

std::size_t VirtualColumnIndex::chunk_of(std::size_t at) const noexcept
{
    const auto found = std::ranges::upper_bound(chunks_, at, {}, &VirtualColumnChunk::start);
    return static_cast<std::size_t>(std::distance(chunks_.begin(), found)) - 1;
}

VirtualColumnCursor VirtualColumnIndex::cursor_at(const TextBuffer &text, Offset at) const
{
    if (chunks_.empty())
    {
        return zero_cursor();
    }
    const auto clamped = std::min(at.value, size_bytes());
    const auto &chunk = chunks_.at(chunk_of(clamped));
    if (clamped == chunk.start)
    {
        return start_of(chunk);
    }
    const auto from = line_start_within(text, chunk, clamped);
    auto state = from == chunk.start ? start_of(chunk) : zero_cursor();
    // plain の塊は 1 バイト＝1 桁＝1 升＝1 コードポイントで、行頭（改行の索引）から引き算する。
    if (chunk.plain)
    {
        const auto inside = clamped - from;
        return VirtualColumnCursor{state.column + inside, state.cell + inside, 0,
                                   state.code_point + inside, 0};
    }
    const auto bytes = text.text_range(Offset{from}, Offset{clamped});
    state.scanned = 1;
    state.copied = bytes.size();
    static_cast<void>(walked(bytes, state, columns_));
    return state;
}

std::size_t VirtualColumnIndex::virtual_column_at(const TextBuffer &text, Offset at) const
{
    return cursor_at(text, at).column;
}

std::size_t VirtualColumnIndex::code_point_at(const TextBuffer &text, Offset at) const
{
    return cursor_at(text, at).code_point;
}

std::size_t VirtualColumnIndex::scanned_until(std::string_view bytes, VirtualColumnCursor state,
                                              std::size_t target, Measure measure) const
{
    for (std::size_t at = 0; at < bytes.size(); at = next_code_point(bytes, Offset{at}).value)
    {
        const auto before = state.*measure;
        const auto cell = stepped(state, core::code_point_at(bytes, Offset{at}), columns_);
        const auto begin = measure == &VirtualColumnCursor::cell ? cell : before;
        const auto end = state.*measure;
        if (end > begin && target < end)
        {
            return at;
        }
    }
    return bytes.size();
}

std::pair<Offset, std::size_t> VirtualColumnIndex::byte_at(const TextBuffer &text, LineNumber line,
                                                           std::size_t target,
                                                           Measure measure) const
{
    const auto line_begin = text.line_start(line).value;
    const auto line_end = text.line_end(line).value;
    if (chunks_.empty())
    {
        return {Offset{line_begin}, 0};
    }
    // 行頭より後ろで始まる塊の始まりの累積はこの行のもの。target を越えない最後の塊から歩く。
    const auto first = chunk_of(line_begin);
    const auto inside =
        std::ranges::subrange(std::next(chunks_.begin(), signed_of(first + 1)),
                              std::next(chunks_.begin(), signed_of(chunk_of(line_end) + 1)));
    const auto found =
        std::ranges::upper_bound(inside, target, {}, [measure](const VirtualColumnChunk &chunk)
                                 { return start_of(chunk).*measure; });
    const bool from_line = found == inside.begin();
    const auto &chunk = from_line ? chunks_.at(first) : *std::prev(found);
    const auto from = from_line ? line_begin : chunk.start;
    const auto state = from_line ? zero_cursor() : start_of(chunk);
    const auto stop = std::min(chunk.start + chunk.bytes, line_end);
    if (chunk.plain)
    {
        const auto reached = target - state.*measure;
        return {Offset{std::min(from + reached, stop)}, 0};
    }
    const auto bytes = text.text_range(Offset{from}, Offset{stop});
    return {Offset{from + scanned_until(bytes, state, target, measure)}, bytes.size()};
}

Offset VirtualColumnIndex::byte_at_virtual_column(const TextBuffer &text, LineNumber line,
                                                  std::size_t column) const
{
    return byte_at(text, line, column, &VirtualColumnCursor::column).first;
}

Offset VirtualColumnIndex::byte_at_cell(const TextBuffer &text, LineNumber line,
                                        std::size_t cell) const
{
    return byte_at(text, line, cell, &VirtualColumnCursor::cell).first;
}

Offset VirtualColumnIndex::byte_at_code_point(const TextBuffer &text, LineNumber line,
                                              std::size_t ordinal) const
{
    return reached_code_point(text, line, ordinal).first;
}

std::pair<Offset, std::size_t> VirtualColumnIndex::reached_code_point(const TextBuffer &text,
                                                                      LineNumber line,
                                                                      std::size_t ordinal) const
{
    return byte_at(text, line, ordinal, &VirtualColumnCursor::code_point);
}
} // namespace nenenib::core
