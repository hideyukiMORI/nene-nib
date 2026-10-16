#include "VirtualColumnIndex.hpp"

#include "Grid.hpp"
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
#include <vector>

namespace nenenib::core
{
namespace
{
// 塊の大きさ（ADR 0140 の決定 4）。作るときは 64 KiB で切り、編集で伸びた塊は 2 倍を越えたら
// 切り直す。4 分の 1 を下回った塊は次の塊と合わせて切り直す。
constexpr std::size_t chunk_bytes = 64UL * 1024UL;
constexpr std::size_t longest_chunk = 2 * chunk_bytes;
constexpr std::size_t merge_bytes = chunk_bytes / 4;
// 切り目を code point の先頭へ寄せるために余分に読むバイト数（UTF-8 の最長 4 バイト）。
constexpr std::size_t sequence_slack = 3;
constexpr std::size_t tab_stop = 8;
constexpr char32_t tab_character = U'\t';
constexpr unsigned char first_plain = 0x20;
constexpr unsigned char last_plain = 0x7E;
constexpr unsigned char continuation_mask = 0xC0;
constexpr unsigned char continuation_bits = 0x80;

[[nodiscard]] bool is_plain(std::string_view bytes) noexcept
{
    return std::ranges::all_of(bytes,
                               [](char value)
                               {
                                   const auto byte = static_cast<unsigned char>(value);
                                   return byte >= first_plain && byte <= last_plain;
                               });
}

[[nodiscard]] bool continues(char value) noexcept
{
    return (static_cast<unsigned char>(value) & continuation_mask) == continuation_bits;
}

// 最後の改行の位置（無ければ npos）。STL の後ろからの検索は core の外へ別のシンボルを出すので、
// ここで 1 バイトずつ戻る（ARC-003）。
[[nodiscard]] std::size_t last_newline_of(std::string_view bytes) noexcept
{
    for (std::size_t at = bytes.size(); at > 0; --at)
    {
        if (bytes[at - 1] == '\n')
        {
            return at - 1;
        }
    }
    return std::string_view::npos;
}

[[nodiscard]] std::size_t next_stop(std::size_t column) noexcept
{
    return ((column / tab_stop) + 1) * tab_stop;
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

// '\n' を含まず plain でもない塊の要約。最初の Tab の手前と後ろを分けて歩く。
void summarize_line_part(VirtualColumnChunk &chunk, std::string_view bytes, std::size_t columns)
{
    VirtualColumnCursor state{chunk.column, chunk.cell, 0};
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
}

// [start, start + bytes) を state から始まる塊として要約する。'\n' を含む塊は最後の '\n' の
// 後ろだけを歩く（そこより前の桁は次の塊に効かない）。
[[nodiscard]] VirtualColumnChunk summarized(std::string_view bytes, std::size_t start,
                                            VirtualColumnCursor state, std::size_t columns)
{
    VirtualColumnChunk chunk{};
    chunk.start = start;
    chunk.bytes = bytes.size();
    chunk.column = state.column;
    chunk.cell = state.cell;
    chunk.plain = is_plain(bytes);
    if (chunk.plain)
    {
        chunk.lead = bytes.size();
        return chunk;
    }
    const auto last_newline = last_newline_of(bytes);
    if (last_newline == std::string_view::npos)
    {
        summarize_line_part(chunk, bytes, columns);
        return chunk;
    }
    VirtualColumnCursor tail{0, 0, 0};
    static_cast<void>(walked(bytes.substr(last_newline + 1), tail, columns));
    chunk.resets = true;
    chunk.rest = tail.column;
    chunk.tail_cell = tail.cell;
    return chunk;
}

// 塊の終わりの累積。塊を読まずに要約から足す（Tab の余りは最初の Tab で補正する）。
[[nodiscard]] VirtualColumnCursor end_of(const VirtualColumnChunk &chunk) noexcept
{
    if (chunk.resets)
    {
        return VirtualColumnCursor{chunk.rest, chunk.tail_cell, 0};
    }
    const auto column =
        chunk.tab ? next_stop(chunk.column + chunk.lead) + chunk.rest : chunk.column + chunk.lead;
    return VirtualColumnCursor{column, chunk.cell + (column - chunk.column) + chunk.padding, 0};
}

// 累積の変わらない塊を、始まりのバイト位置だけずらして足す。
void append_shifted(std::vector<VirtualColumnChunk> &out, std::span<const VirtualColumnChunk> rest,
                    std::size_t shift)
{
    std::ranges::transform(rest, std::back_inserter(out),
                           [shift](VirtualColumnChunk moved)
                           {
                               moved.start += shift;
                               return moved;
                           });
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
    return state.column % tab_stop != chunk.column % tab_stop ||
           state.cell % columns != chunk.cell % columns;
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
    VirtualColumnCursor state{0, 0, 0};
    std::size_t start = 0;
    while (start < text.size_bytes())
    {
        const auto stop = std::min(start + chunk_bytes + sequence_slack, text.size_bytes());
        const auto bytes = text.text_range(Offset{start}, Offset{stop});
        const auto cut = cut_point(bytes, chunk_bytes);
        index.chunks_.push_back(
            summarized(std::string_view(bytes).substr(0, cut), start, state, columns));
        state = end_of(index.chunks_.back());
        start += cut;
    }
    index.rebuilt_ = index.chunks_.size();
    return index;
}

VirtualColumnCursor VirtualColumnIndex::append_cut(const TextBuffer &text, std::size_t begin,
                                                   std::size_t end, VirtualColumnCursor state)
{
    const auto bytes = text.text_range(Offset{begin}, Offset{end});
    const auto pieces = (bytes.size() + longest_chunk - 1) / longest_chunk;
    const auto target = pieces == 0 ? 0 : (bytes.size() + pieces - 1) / pieces;
    const std::string_view view(bytes);
    std::size_t from = 0;
    while (from < view.size())
    {
        const auto cut = cut_point(view, from + target);
        chunks_.push_back(summarized(view.substr(from, cut - from), begin + from, state, columns_));
        state = end_of(chunks_.back());
        ++rebuilt_;
        from = cut;
    }
    return state;
}

void VirtualColumnIndex::append_carried(const VirtualColumnIndex &old, std::size_t from,
                                        std::size_t shift, const TextBuffer &text)
{
    auto state = chunks_.empty() ? VirtualColumnCursor{0, 0, 0} : end_of(chunks_.back());
    for (auto at = from; at < old.chunks_.size(); ++at)
    {
        auto chunk = old.chunks_.at(at);
        chunk.start += shift;
        // 始まりが前と同じなら、ここから後ろの累積は変わらない（ずらすだけ）。
        if (state.column == chunk.column && state.cell == chunk.cell)
        {
            append_shifted(chunks_, std::span(old.chunks_).subspan(at), shift);
            return;
        }
        if (needs_rescan(chunk, state, columns_))
        {
            const auto bytes =
                text.text_range(Offset{chunk.start}, Offset{chunk.start + chunk.bytes});
            chunk = summarized(bytes, chunk.start, state, columns_);
            ++rebuilt_;
        }
        chunk.column = state.column;
        chunk.cell = state.cell;
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
    const auto state = first == 0 ? VirtualColumnCursor{0, 0, 0} : end_of(result.chunks_.back());
    static_cast<void>(result.append_cut(after, span_begin, new_end, state));
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
        return VirtualColumnCursor{0, 0, 0};
    }
    const auto clamped = std::min(at.value, size_bytes());
    const auto &chunk = chunks_.at(chunk_of(clamped));
    const auto inside = clamped - chunk.start;
    if (chunk.plain || inside == 0)
    {
        return VirtualColumnCursor{chunk.column + inside, chunk.cell + inside, 0};
    }
    const auto bytes = text.text_range(Offset{chunk.start}, Offset{clamped});
    const auto newline = last_newline_of(bytes);
    const bool restarts = newline != std::string_view::npos;
    VirtualColumnCursor state{restarts ? 0 : chunk.column, restarts ? 0 : chunk.cell, 1};
    static_cast<void>(
        walked(std::string_view(bytes).substr(restarts ? newline + 1 : 0), state, columns_));
    return state;
}

std::size_t VirtualColumnIndex::virtual_column_at(const TextBuffer &text, Offset at) const
{
    return cursor_at(text, at).column;
}

std::size_t VirtualColumnIndex::scanned_until(std::string_view bytes, VirtualColumnCursor state,
                                              std::size_t target, bool by_cell) const
{
    for (std::size_t at = 0; at < bytes.size(); at = next_code_point(bytes, Offset{at}).value)
    {
        const auto column = state.column;
        const auto cell = stepped(state, code_point_at(bytes, Offset{at}), columns_);
        const auto begin = by_cell ? cell : column;
        const auto end = by_cell ? state.cell : state.column;
        if (end > begin && target < end)
        {
            return at;
        }
    }
    return bytes.size();
}

Offset VirtualColumnIndex::byte_at(const TextBuffer &text, LineNumber line, std::size_t target,
                                   bool by_cell) const
{
    const auto line_begin = text.line_start(line).value;
    const auto line_end = text.line_end(line).value;
    if (chunks_.empty())
    {
        return Offset{line_begin};
    }
    // 行頭より後ろで始まる塊の始まりの累積はこの行のもの。target を越えない最後の塊から歩く。
    const auto first = chunk_of(line_begin);
    const auto inside =
        std::ranges::subrange(std::next(chunks_.begin(), signed_of(first + 1)),
                              std::next(chunks_.begin(), signed_of(chunk_of(line_end) + 1)));
    const auto found =
        std::ranges::upper_bound(inside, target, {}, [by_cell](const VirtualColumnChunk &chunk)
                                 { return by_cell ? chunk.cell : chunk.column; });
    const bool from_line = found == inside.begin();
    const auto &chunk = from_line ? chunks_.at(first) : *std::prev(found);
    const auto from = from_line ? line_begin : chunk.start;
    const VirtualColumnCursor state{from_line ? 0 : chunk.column, from_line ? 0 : chunk.cell, 0};
    const auto stop = std::min(chunk.start + chunk.bytes, line_end);
    if (chunk.plain)
    {
        const auto reached = target - (by_cell ? state.cell : state.column);
        return Offset{std::min(from + reached, stop)};
    }
    const auto bytes = text.text_range(Offset{from}, Offset{stop});
    return Offset{from + scanned_until(bytes, state, target, by_cell)};
}

Offset VirtualColumnIndex::byte_at_virtual_column(const TextBuffer &text, LineNumber line,
                                                  std::size_t column) const
{
    return byte_at(text, line, column, false);
}

Offset VirtualColumnIndex::byte_at_cell(const TextBuffer &text, LineNumber line,
                                        std::size_t cell) const
{
    return byte_at(text, line, cell, true);
}
} // namespace nenenib::core
