#include "WordTable.hpp"

#include "DisplayWidth.hpp"
#include "LineNumber.hpp"
#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimKeywordRuns.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <utility>

namespace nenenib::core
{
namespace
{
using Counts = std::vector<WordCount>;

// 編集の前後を読む窓の最初の幅。語の文字の連なりが窓より長ければ倍にして読み直す。
constexpr std::size_t first_reach = 64;
// 1 つの code point の最大のバイト数（UTF-8）。
constexpr std::size_t code_point_bytes = 4;

[[nodiscard]] std::string_view word_of(const WordCount &entry) noexcept
{
    return entry.word;
}

// content の 2 文字以上の語（ADR 0169 の決定 1）。
[[nodiscard]] std::vector<std::string_view> words_of(std::string_view content)
{
    std::vector<std::string_view> words;
    for (const OffsetRange &run : vim_keyword_runs(content))
    {
        if (vim_keyword_has_two_characters(content, run))
        {
            words.push_back(content.substr(run.begin.value, run.end.value - run.begin.value));
        }
    }
    return words;
}

// 表を作る間の 1 行: 語の先頭 8 バイトを大きい桁から詰めた鍵・語・数。tuple の < は鍵で比べ、
// 鍵が同じときだけ語のバイト列で比べるので、並びは語のバイト列の順と同じ（#1872）。
using Tally = std::tuple<std::uint64_t, std::string_view, std::int64_t>;
// 語 → Tally の列の位置の開番地の表（線形の探索）。大きさは語の数の 2 倍以上の 2 の冪なので、
// 埋まりは半分までで作り直さない。1 枠は上位 32 ビットが語の hash の上位、
// 下位 32 ビットが位置 + 1（0 は空の枠）。
using Slots = std::vector<std::uint64_t>;

constexpr std::uint64_t empty_slot = 0;
constexpr std::size_t least_slots = 16;
constexpr std::size_t key_bytes = sizeof(std::uint64_t);
constexpr unsigned byte_bits = 8U;
constexpr std::uint64_t low_half = 0xFFFF'FFFFULL;
constexpr std::uint64_t fnv_offset = 14695981039346656037ULL;
constexpr std::uint64_t fnv_prime = 1099511628211ULL;

[[nodiscard]] std::uint64_t order_key(std::string_view word) noexcept
{
    std::uint64_t key = 0;
    for (std::size_t at = 0; at < key_bytes; ++at)
    {
        const std::uint64_t byte = at < word.size() ? static_cast<unsigned char>(word[at]) : 0U;
        key = (key << byte_bits) | byte;
    }
    return key;
}

// FNV-1a（語の表の中だけで使う・外へ出さない）。
[[nodiscard]] std::uint64_t hashed(std::string_view word) noexcept
{
    std::uint64_t hash = fnv_offset;
    for (const char unit : word)
    {
        hash = (hash ^ static_cast<unsigned char>(unit)) * fnv_prime;
    }
    return hash;
}

[[nodiscard]] std::size_t slot_count(std::size_t words) noexcept
{
    std::size_t count = least_slots;
    while (count < words * 2)
    {
        count *= 2;
    }
    return count;
}

// 語の列の種類ごとの数（出てきた順）。数は 1 語につき sign。
[[nodiscard]] std::vector<Tally> tallied(std::span<const std::string_view> words, std::int64_t sign)
{
    std::vector<Tally> tallies;
    Slots slots(slot_count(words.size()), empty_slot);
    const std::size_t mask = slots.size() - 1;
    for (const std::string_view word : words)
    {
        const std::uint64_t hash = hashed(word);
        const std::uint64_t tag = hash & ~low_half;
        std::size_t at = static_cast<std::size_t>(hash) & mask;
        while (slots[at] != empty_slot &&
               ((slots[at] & ~low_half) != tag ||
                std::get<1>(tallies[(slots[at] & low_half) - 1]) != word))
        {
            at = (at + 1) & mask;
        }
        if (slots[at] == empty_slot)
        {
            tallies.emplace_back(order_key(word), word, 0);
            slots[at] = tag | tallies.size();
        }
        std::get<2>(tallies[(slots[at] & low_half) - 1]) += sign;
    }
    return tallies;
}

// 語の列を語の順の行にまとめる。数は 1 語につき sign。語ごとの std::string は種類ごとに 1 回だけ
// 作り、並べるのは種類の列だけ（#1872）。
[[nodiscard]] Counts counted(std::span<const std::string_view> words, std::int64_t sign)
{
    std::vector<Tally> tallies = tallied(words, sign);
    std::ranges::sort(tallies);
    Counts counts;
    counts.reserve(tallies.size());
    for (const Tally &tally : tallies)
    {
        counts.push_back(WordCount{std::string{std::get<1>(tally)}, std::get<2>(tally)});
    }
    return counts;
}

// 語の順の 2 つの列を前から 1 行ずつ併せて歩く。同じ語は数を足した 1 行にする。
[[nodiscard]] WordCount next_merged(std::span<const WordCount> left, std::size_t &at_left,
                                    std::span<const WordCount> right, std::size_t &at_right)
{
    const bool take_left = at_right == right.size() ||
                           (at_left < left.size() && left[at_left].word <= right[at_right].word);
    const bool take_right = at_left == left.size() ||
                            (at_right < right.size() && right[at_right].word <= left[at_left].word);
    WordCount entry = take_left ? left[at_left] : right[at_right];
    if (take_left && take_right)
    {
        entry.count += right[at_right].count;
    }
    at_left += take_left ? 1 : 0;
    at_right += take_right ? 1 : 0;
    return entry;
}

// 語の順の 2 つの列の和。数が 0 になった行は落とす。
[[nodiscard]] Counts merged(const Counts &left, const Counts &right)
{
    Counts sum;
    sum.reserve(left.size() + right.size());
    std::size_t at_left = 0;
    std::size_t at_right = 0;
    while (at_left < left.size() || at_right < right.size())
    {
        WordCount entry = next_merged(left, at_left, right, at_right);
        if (entry.count != 0)
        {
            sum.push_back(std::move(entry));
        }
    }
    return sum;
}

[[nodiscard]] bool is_separator(std::string_view window, std::size_t at) noexcept
{
    return !vim_keyword_group(vim_keyword_class_at(window, at));
}

// 窓 window の pos から手前へ、語の文字でない 1 文字の先頭（窓の先頭が行頭 line_head なら行頭）を
// 探す。窓の先頭まで語の文字が続いて答えが決まらなければ値なし。
[[nodiscard]] std::optional<std::size_t> separator_before(std::string_view window, std::size_t pos,
                                                          bool line_head)
{
    while (pos > 0)
    {
        const std::size_t start = vim_character_start(window, Offset{pos}).value;
        if (start == 0 && !line_head)
        {
            return std::nullopt;
        }
        if (is_separator(window, start))
        {
            return start;
        }
        pos = start;
    }
    return line_head ? std::optional<std::size_t>{0} : std::nullopt;
}

// 編集の始まり at を含む語の手前の境（本文の位置）。境は行頭か、語の文字でない 1 文字の先頭で、
// そこより前は編集の後も同じ文字の切れ方をする。
[[nodiscard]] Offset left_boundary(const TextBuffer &text, Offset at)
{
    const std::size_t line = text.line_start(text.line_containing(at)).value;
    const std::size_t upto = std::min(text.size_bytes(), at.value + code_point_bytes);
    for (std::size_t reach = first_reach;; reach *= 2)
    {
        const std::size_t from = at.value - std::min(reach, at.value - line);
        std::string window = text.text_range(Offset{from}, Offset{upto});
        window.resize(whole_code_points(window));
        std::size_t pos = at.value - from;
        if (vim_character_continues(window, Offset{pos}))
        {
            pos = vim_character_start(window, Offset{pos}).value;
        }
        const auto found = separator_before(window, pos, from == line);
        if (found.has_value())
        {
            return Offset{from + found.value()};
        }
    }
}

// 境として使える 1 文字か: 語の文字でなく、幅 0 でない（幅 0 の code point は編集で手前が
// 変わると手前の文字に付く）。
[[nodiscard]] bool fixed_separator(std::string_view window, std::size_t at) noexcept
{
    return is_separator(window, at) &&
           display_width(code_point_at(window, Offset{at})) != DisplayWidth::zero;
}

// 窓 window の pos から後ろへ、境として使える 1 文字の先頭を探す。窓の終わりまで決まらなければ
// 値なし（窓の終わりが行の終わり tail なら、そこが答え）。
[[nodiscard]] std::optional<std::size_t> separator_after(std::string_view window, std::size_t pos,
                                                         bool tail)
{
    while (pos < window.size())
    {
        if (fixed_separator(window, pos))
        {
            return pos;
        }
        pos = vim_character_end(window, Offset{pos}).value;
    }
    return tail ? std::optional<std::size_t>{window.size()} : std::nullopt;
}

// 編集の終わり end（編集の前の本文の上）を含む語の後ろの境。境の文字から後ろは編集の後も同じ
// 文字の切れ方をする。
[[nodiscard]] Offset right_boundary(const TextBuffer &text, Offset end)
{
    const LineNumber line = text.line_containing(end);
    const std::size_t stop = text.line_end(line).value;
    if (end.value >= stop)
    {
        return end;
    }
    const std::size_t from =
        end.value - std::min(code_point_bytes, end.value - text.line_start(line).value);
    for (std::size_t reach = first_reach;; reach *= 2)
    {
        const std::size_t upto = std::min(stop, end.value + reach);
        std::string window = text.text_range(Offset{from}, Offset{upto});
        window.resize(whole_code_points(window));
        std::size_t pos = end.value - from;
        if (vim_character_continues(window, Offset{pos}))
        {
            pos = vim_character_end(window, vim_character_start(window, Offset{pos})).value;
        }
        const auto found = separator_after(window, pos, upto == stop);
        if (found.has_value())
        {
            return Offset{from + found.value()};
        }
    }
}

[[nodiscard]] Counts::const_iterator first_prefixed(const Counts &counts, std::string_view prefix)
{
    return std::ranges::lower_bound(counts, prefix, std::less<>{}, word_of);
}

[[nodiscard]] std::int64_t count_in(const Counts &counts, std::string_view word)
{
    const auto found = first_prefixed(counts, word);
    return found != counts.end() && found->word == word ? found->count : 0;
}

// counts のうち prefix で始まる行（語の順に連続する 1 区間）。
[[nodiscard]] std::span<const WordCount> prefixed_span(const Counts &counts,
                                                       std::string_view prefix)
{
    const auto first = first_prefixed(counts, prefix);
    const auto last = std::find_if_not(first, counts.end(), [prefix](const WordCount &entry)
                                       { return entry.word.starts_with(prefix); });
    return {first, last};
}
} // namespace

WordTable::WordTable(std::shared_ptr<const Counts> base, std::shared_ptr<const Counts> recent)
    : base_(std::move(base)), recent_(std::move(recent))
{
}

std::optional<WordTable> WordTable::of(const TextBuffer &text)
{
    if (text.size_bytes() > word_table_byte_limit)
    {
        return std::nullopt;
    }
    const std::string content = text.text();
    return WordTable(std::make_shared<const Counts>(counted(words_of(content), 1)),
                     std::make_shared<const Counts>());
}

WordTable WordTable::edited(const TextBuffer &before, const Edit &edit) const
{
    const Offset end{edit.at.value + edit.removed.size()};
    const Offset begin = left_boundary(before, edit.at);
    const Offset stop = right_boundary(before, end);
    const std::string left = before.text_range(begin, edit.at);
    const std::string right = before.text_range(end, stop);
    // 編集の前の語を 1 つずつ引き、後の語を 1 つずつ足した増減（同じ語は打ち消し合う）。
    const Counts change = merged(counted(words_of(left + edit.removed + right), -1),
                                 counted(words_of(left + edit.inserted + right), 1));
    if (change.empty())
    {
        return *this;
    }
    Counts recent = merged(*recent_, change);
    if (recent.size() <= recent_word_limit)
    {
        return WordTable(base_, std::make_shared<const Counts>(std::move(recent)));
    }
    return WordTable(std::make_shared<const Counts>(merged(*base_, recent)),
                     std::make_shared<const Counts>());
}

std::int64_t WordTable::count(std::string_view word) const
{
    return count_in(*base_, word) + count_in(*recent_, word);
}

std::vector<WordCount> WordTable::prefixed(std::string_view prefix, std::size_t limit) const
{
    const std::span<const WordCount> base = prefixed_span(*base_, prefix);
    const std::span<const WordCount> recent = prefixed_span(*recent_, prefix);
    std::vector<WordCount> found;
    std::size_t at_base = 0;
    std::size_t at_recent = 0;
    while (found.size() < limit && (at_base < base.size() || at_recent < recent.size()))
    {
        WordCount entry = next_merged(base, at_base, recent, at_recent);
        if (entry.count > 0)
        {
            found.push_back(std::move(entry));
        }
    }
    return found;
}

std::size_t WordTable::recent_size() const noexcept
{
    return recent_->size();
}
} // namespace nenenib::core
