#include "CompletionCandidates.hpp"

#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimKeywordRuns.hpp"

#include <algorithm>
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
// 候補の列と、出した語の集合（語の順・二分探索で重複を落とす）。
using Collected = std::pair<CompletionList, std::vector<std::string>>;

[[nodiscard]] bool keyword_at(std::string_view content, std::size_t at) noexcept
{
    return vim_keyword_group(vim_keyword_class_at(content, at));
}

// 接頭辞が Vim の 1 文字を 2 つ以上持つか。
[[nodiscard]] bool two_characters(std::string_view prefix) noexcept
{
    return !prefix.empty() && vim_character_end(prefix, Offset{0}).value < prefix.size();
}

// 語の頭 run が Vim の照合（接頭辞が 2 文字以上なら `\<prefix`、0〜1 文字なら続く 1 文字も
// 語の文字 `\<prefix\k`）に当たるか。
[[nodiscard]] bool run_matches(std::string_view content, const OffsetRange &run,
                               std::string_view prefix)
{
    // 2 文字以上の接頭辞の終わりが本文の Vim の 1 文字の途中なら当たらない（`aa` は `aá` の頭に
    // 当たらない）。1 文字の接頭辞は続く `\k` が結合文字を食うので当たる（`b` は `b́`）。
    if (!content.substr(run.begin.value).starts_with(prefix))
    {
        return false;
    }
    if (two_characters(prefix))
    {
        return !vim_character_continues(content, Offset{run.begin.value + prefix.size()});
    }
    const std::size_t head = prefix.empty() ? vim_character_end(content, run.begin).value
                                            : run.begin.value + prefix.size();
    return head < run.end.value || keyword_at(content, head);
}

// 走る窓の [first, last) の内側の語の頭のうち、窓の端で切れたかもしれないもの（窓の先頭から始まる・
// 窓の終わりで終わる）を落とす。窓の端が本文の端なら落とさない。
[[nodiscard]] std::vector<OffsetRange> whole_runs(std::string_view content, bool cut_head,
                                                  bool cut_tail)
{
    std::vector<OffsetRange> runs = vim_keyword_runs(content);
    if (cut_tail && !runs.empty() && runs.back().end.value == content.size())
    {
        runs.pop_back();
    }
    if (cut_head && !runs.empty() && runs.front().begin.value == 0)
    {
        runs.erase(runs.begin());
    }
    return runs;
}

// 出していない語なら列に足す。列が上限に達したら偽。
bool offer(std::string_view word, CompletionSource source, Collected &collected)
{
    auto &[list, seen] = collected;
    if (list.candidates.size() >= completion_candidate_limit)
    {
        return false;
    }
    const auto place =
        std::ranges::lower_bound(seen, word, std::less<>{},
                                 [](const std::string &entry) { return std::string_view{entry}; });
    if (place == seen.end() || *place != word)
    {
        seen.insert(place, std::string{word});
        list.candidates.push_back(CompletionCandidate{std::string{word}, source});
    }
    return list.candidates.size() < completion_candidate_limit;
}

// 当たった語の頭 matched（本文の順）を向きの順に列へ足す。接頭辞の位置より後ろと前に分け、
// forward は後ろ→前の本文の順、backward は前→後ろの本文の逆順（Vim の折り返す検索の順）。
void offer_runs(std::string_view content, std::span<const OffsetRange> matched,
                std::pair<std::size_t, CompletionDirection> start, Collected &collected)
{
    const auto split = std::ranges::partition_point(
        matched, [at = start.first](const OffsetRange &run) { return run.begin.value < at; });
    const std::span<const OffsetRange> before{matched.begin(), split};
    const std::span<const OffsetRange> after{split, matched.end()};
    std::vector<OffsetRange> order;
    order.reserve(matched.size());
    switch (start.second)
    {
    case CompletionDirection::forward:
        order.insert(order.end(), after.begin(), after.end());
        order.insert(order.end(), before.begin(), before.end());
        break;
    case CompletionDirection::backward:
        order.insert(order.end(), before.rbegin(), before.rend());
        order.insert(order.end(), after.rbegin(), after.rend());
        break;
    }
    for (const OffsetRange &run : order)
    {
        const std::string_view word =
            content.substr(run.begin.value, run.end.value - run.begin.value);
        if (!offer(word, CompletionSource::buffer, collected))
        {
            return;
        }
    }
}

// 表 table の接頭辞で始まる語を出どころ source で足す。skipped は表の数から 1 つ引く語（今の
// 文書の接頭辞の位置の語・Vim はそこを候補にしない）。
void offer_table(const WordTable &table, std::string_view prefix,
                 std::pair<CompletionSource, std::string_view> skipped, Collected &collected)
{
    const std::size_t wanted = completion_candidate_limit + collected.second.size() + 1;
    for (const WordCount &entry : table.prefixed(prefix, wanted))
    {
        if (two_characters(prefix) && vim_character_continues(entry.word, Offset{prefix.size()}))
        {
            continue;
        }
        const std::int64_t count = entry.count - (entry.word == skipped.second ? 1 : 0);
        if (count > 0 && !offer(entry.word, skipped.first, collected))
        {
            return;
        }
    }
}

// 照合が食う所の終わり。空の接頭辞の `\<\k\k` は 2 文字、ほかは接頭辞の長さ。Vim の検索は
// 当たった所の後ろから次を探す（cpoptions の c）ので、その内側から始まる語は当たらない。
[[nodiscard]] std::size_t consumed_end(std::string_view content, const OffsetRange &run,
                                       std::string_view prefix) noexcept
{
    if (!prefix.empty())
    {
        return run.begin.value + prefix.size();
    }
    return vim_character_end(content, vim_character_end(content, run.begin)).value;
}

// 照合に当たった語の頭（本文の順・oracle の実測）。打っている語（接頭辞の位置から始まる語）は
// 候補にせず、その代わりにキャレットから後ろ（打っている語の残り）を語の頭として 1 回照合する
// （`fo|fooqux` は `fooqux`）。空の接頭辞ではキャレットから始まる語も候補で、キャレットの後ろに
// 数える。当たった所が食った文字の内側から始まる語は当たらない（`日本語の補完` の `補完`）。
[[nodiscard]] std::vector<OffsetRange>
matched_runs(std::string_view content, std::span<const OffsetRange> runs, const OffsetRange &typed)
{
    const std::string_view prefix =
        content.substr(typed.begin.value, typed.end.value - typed.begin.value);
    std::vector<OffsetRange> matched;
    std::size_t consumed = 0;
    for (const OffsetRange &run : runs)
    {
        const OffsetRange tried =
            !prefix.empty() && run.begin == typed.begin ? OffsetRange{typed.end, run.end} : run;
        if (tried.begin.value >= consumed && tried.begin.value < tried.end.value &&
            run_matches(content, tried, prefix))
        {
            matched.push_back(tried);
            consumed = consumed_end(content, tried, prefix);
        }
    }
    return matched;
}

// 照合の連なりに食われた語を拾い直す（Vim の complete の `i` が今の文書をもう 1 度走り、語ごとに
// 照合する・oracle の実測）。キャレットの行の外の、単独で照合に当たる語の頭を本文の順に返す。
// キャレットの行は拾い直さない（`日本語の補完は便利です |` は 4 語・別の行なら `補完` `便利` も）。
[[nodiscard]] std::vector<OffsetRange> unchained_runs(std::string_view content,
                                                      std::span<const OffsetRange> runs,
                                                      const OffsetRange &typed)
{
    const std::string_view prefix =
        content.substr(typed.begin.value, typed.end.value - typed.begin.value);
    const std::size_t at = typed.end.value;
    const std::size_t newline = at == 0 ? std::string_view::npos : content.rfind('\n', at - 1);
    const std::size_t line = newline == std::string_view::npos ? 0 : newline + 1;
    const std::size_t stop = std::min(content.find('\n', at), content.size());
    std::vector<OffsetRange> found;
    for (const OffsetRange &run : runs)
    {
        const bool caret_line = run.begin.value >= line && run.begin.value <= stop;
        if (!caret_line && run_matches(content, run, prefix))
        {
            found.push_back(run);
        }
    }
    return found;
}

// 接頭辞の位置から始まる語（無ければ空）。
[[nodiscard]] std::string_view word_at(std::string_view content, std::span<const OffsetRange> runs,
                                       std::size_t at)
{
    const auto found =
        std::ranges::find_if(runs, [at](const OffsetRange &run) { return run.begin.value == at; });
    return found == runs.end() ? std::string_view{}
                               : content.substr(at, found->end.value - found->begin.value);
}
} // namespace

std::size_t completion_prefix_start(std::string_view content, std::size_t caret) noexcept
{
    if (caret == 0)
    {
        return caret;
    }
    std::size_t start = vim_character_start(content, Offset{caret}).value;
    const std::uint32_t group = vim_keyword_class_at(content, start);
    if (!vim_keyword_group(group))
    {
        return caret;
    }
    while (start > 0)
    {
        const std::size_t previous = vim_character_start(content, Offset{start}).value;
        if (vim_keyword_class_at(content, previous) != group)
        {
            break;
        }
        start = previous;
    }
    return start;
}

CompletionList completion_list(const TextBuffer &text, Offset caret, CompletionDirection direction,
                               const CompletionTables &tables)
{
    const std::size_t from = caret.value - std::min(caret.value, completion_scan_bytes);
    const std::size_t upto = std::min(text.size_bytes(), caret.value + completion_scan_bytes);
    const bool whole = from == 0 && upto == text.size_bytes();
    std::string window = text.text_range(Offset{from}, Offset{upto});
    window.resize(upto < text.size_bytes() ? whole_code_points(window) : window.size());
    const std::size_t first = from > 0 ? leading_continuations(window) : 0;
    const std::string_view content = std::string_view{window}.substr(first);
    const std::size_t at = caret.value - from - first;
    const std::size_t start = completion_prefix_start(content, at);
    const std::string prefix{content.substr(start, at - start)};
    Collected collected{CompletionList{OffsetRange{Offset{from + first + start}, caret}, {}}, {}};
    const std::vector<OffsetRange> runs = whole_runs(content, from > 0, upto < text.size_bytes());
    const OffsetRange typed_range{Offset{start}, Offset{at}};
    offer_runs(content, matched_runs(content, runs, typed_range), {start, direction}, collected);
    // 拾い直した語は列の後ろに、forward は本文の順・backward は逆順で足す（位置 0 で分ければ全部が
    // 「後ろ」になる）。
    offer_runs(content, unchained_runs(content, runs, typed_range), {0, direction}, collected);
    if (tables.own != nullptr && !whole)
    {
        const std::string_view typed =
            prefix.empty() ? std::string_view{} : word_at(content, runs, start);
        offer_table(*tables.own, prefix, {CompletionSource::buffer, typed}, collected);
    }
    for (const WordTable *other : tables.others)
    {
        offer_table(*other, prefix, {CompletionSource::other_buffers, {}}, collected);
    }
    return std::move(collected.first);
}
} // namespace nenenib::core
