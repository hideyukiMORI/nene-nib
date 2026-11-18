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

// 語の頭 run が Vim の照合（接頭辞が 2 文字以上なら `\<prefix`、0〜1 文字なら続く 1 文字も
// 語の文字 `\<prefix\k`）に当たるか。
[[nodiscard]] bool run_matches(std::string_view content, const OffsetRange &run,
                               std::string_view prefix)
{
    if (!content.substr(run.begin.value).starts_with(prefix))
    {
        return false;
    }
    const std::size_t head = prefix.empty() ? vim_character_end(content, run.begin).value
                                            : run.begin.value + prefix.size();
    if (!prefix.empty() && vim_character_end(prefix, Offset{0}).value < prefix.size())
    {
        return true;
    }
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
        const std::int64_t count = entry.count - (entry.word == skipped.second ? 1 : 0);
        if (count > 0 && !offer(entry.word, skipped.first, collected))
        {
            return;
        }
    }
}

// 照合に当たった語の頭（本文の順）。接頭辞が空でなければ、接頭辞の位置 start から始まる語（打って
// いる語そのもの）は除く。空の接頭辞ではキャレットから始まる語も候補で、キャレットの後ろに数える
// （oracle の実測。Vim は検索を接頭辞の 1 文字手前から始める）。
[[nodiscard]] std::vector<OffsetRange> matched_runs(std::string_view content,
                                                    std::span<const OffsetRange> runs,
                                                    std::size_t start, std::string_view prefix)
{
    std::vector<OffsetRange> matched;
    for (const OffsetRange &run : runs)
    {
        if ((prefix.empty() || run.begin.value != start) && run_matches(content, run, prefix))
        {
            matched.push_back(run);
        }
    }
    return matched;
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
    if (tables.own == nullptr)
    {
        return std::move(collected.first);
    }
    const std::vector<OffsetRange> runs = whole_runs(content, from > 0, upto < text.size_bytes());
    offer_runs(content, matched_runs(content, runs, start, prefix), {start, direction}, collected);
    if (!whole)
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
