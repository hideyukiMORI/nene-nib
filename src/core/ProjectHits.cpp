#include "ProjectHits.hpp"

#include "Offset.hpp"
#include "ProjectSearchRules.hpp"
#include "TextEncoding.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::core
{
namespace
{
// 行（`\n` を除いたバイト列）の照合する本文: 1 行目なら UTF-8 の BOM を除き、行末の CR を除く。
[[nodiscard]] std::string_view content_of(std::string_view line, LineNumber number) noexcept
{
    if (number.value == 1 && line.starts_with(byte_order_mark()))
    {
        line.remove_prefix(byte_order_mark().size());
    }
    if (line.ends_with('\r'))
    {
        line.remove_suffix(1);
    }
    return line;
}

// 当たりを見せる窓（ProjectHit の line_text の規則）。content は正しい UTF-8。
[[nodiscard]] ProjectHit hit_of(std::string_view content, LineNumber number,
                                const VimPatternMatch &match)
{
    std::size_t start = 0;
    if (match.end > project_hit_text_bytes)
    {
        start =
            match.begin > project_hit_context_bytes ? match.begin - project_hit_context_bytes : 0;
        while (!is_boundary(content, Offset{start}))
        {
            --start;
        }
    }
    std::string_view window = content.substr(start, project_hit_text_bytes);
    if (start + window.size() < content.size())
    {
        window = window.substr(0, whole_code_points(window));
    }
    const std::size_t column = code_point_count(content.substr(0, match.begin)) + 1;
    return ProjectHit{number,
                      Column{column},
                      std::string(window),
                      start,
                      std::min(match.begin - start, window.size()),
                      std::min(match.end - start, window.size())};
}

[[nodiscard]] std::optional<ProjectHit> hit_in_line(std::string_view line, LineNumber number,
                                                    const ProjectQuery &query)
{
    if (line.size() > project_search_line_limit)
    {
        return std::nullopt;
    }
    const std::string_view content = content_of(line, number);
    if (!validate_utf8(content).has_value())
    {
        return std::nullopt;
    }
    const auto match = query.first_in_line(content);
    if (!match.has_value())
    {
        return std::nullopt;
    }
    return hit_of(content, number, match.value());
}

void push_hit(std::vector<ProjectHit> &hits, std::optional<ProjectHit> hit, std::size_t room)
{
    if (hit.has_value() && hits.size() < room)
    {
        hits.push_back(std::move(hit).value());
    }
}

[[nodiscard]] std::size_t newlines_in(std::string_view text, std::size_t begin, std::size_t end)
{
    return static_cast<std::size_t>(std::count(text.begin() + static_cast<std::ptrdiff_t>(begin),
                                               text.begin() + static_cast<std::ptrdiff_t>(end),
                                               '\n'));
}

// at 以降の最初の `\n` の位置（無ければ text.size()）。char_traits の find は memchr を core の
// 外へ出すので、許可シンボルに載っている std の検索で引く（ARC-003）。
[[nodiscard]] std::size_t newline_from(std::string_view text, std::size_t at)
{
    const std::string_view rest = text.substr(at);
    return at + static_cast<std::size_t>(std::ranges::find(rest, '\n') - rest.begin());
}

// end より前の最後の `\n` の位置（無ければ npos）。
[[nodiscard]] std::size_t newline_before(std::string_view text, std::size_t end)
{
    const std::string_view head = text.substr(0, end);
    const auto found = std::ranges::find_last(head, '\n');
    return found.empty() ? std::string_view::npos
                         : static_cast<std::size_t>(found.begin() - head.begin());
}

// body は `\n` で終わる完全な行の列。照合の候補の行だけを確かめ、out.next.line を body の次の
// 行へ進める。
void scan_lines(std::string_view body, const ProjectQuery &query, std::size_t room,
                ProjectChunkHits &out)
{
    std::size_t line = out.next.line.value;
    std::size_t counted = 0;
    std::size_t at = 0;
    while (out.hits.size() < room)
    {
        const auto candidate = query.candidate(body, at);
        if (!candidate.has_value())
        {
            break;
        }
        const std::size_t found = candidate.value();
        const std::size_t before =
            found == at ? std::string_view::npos : newline_before(body, found);
        const std::size_t start = before == std::string_view::npos || before < at ? at : before + 1;
        line += newlines_in(body, counted, start);
        counted = start;
        const std::size_t end = newline_from(body, found);
        push_hit(out.hits, hit_in_line(body.substr(start, end - start), LineNumber{line}, query),
                 room);
        at = end + 1;
    }
    out.next.line = LineNumber{line + newlines_in(body, counted, body.size())};
}

// 前の塊から続く行を、この塊の最初の `\n` までと合わせて照合する。消費したバイト数を返す
// （`\n` が無ければ塊の全部）。
[[nodiscard]] std::size_t continued(ProjectChunkHits &out, std::string_view chunk,
                                    const ProjectQuery &query, std::size_t room)
{
    ProjectScan &next = out.next;
    const std::size_t taken = newline_from(chunk, 0);
    if (!next.skipping && next.carry.size() + taken > project_search_line_limit)
    {
        next.carry.clear();
        next.skipping = true;
    }
    if (!next.skipping)
    {
        next.carry.append(chunk.substr(0, taken));
    }
    if (taken == chunk.size())
    {
        return chunk.size();
    }
    if (!next.skipping)
    {
        push_hit(out.hits, hit_in_line(next.carry, next.line, query), room);
    }
    next.carry.clear();
    next.skipping = false;
    ++next.line.value;
    return taken + 1;
}
} // namespace

ProjectChunkHits project_hits_in(const ProjectScan &scan, std::string_view chunk,
                                 const ProjectQuery &query, std::size_t room)
{
    ProjectChunkHits out{{}, scan};
    std::string_view rest = chunk;
    if (out.next.skipping || !out.next.carry.empty())
    {
        rest.remove_prefix(continued(out, rest, query, room));
    }
    if (!out.next.skipping && out.next.carry.empty())
    {
        const std::size_t last_newline = newline_before(rest, rest.size());
        const std::size_t body = last_newline == std::string_view::npos ? 0 : last_newline + 1;
        scan_lines(rest.substr(0, body), query, room, out);
        const std::string_view tail = rest.substr(body);
        out.next.skipping = tail.size() > project_search_line_limit;
        if (!out.next.skipping)
        {
            out.next.carry.assign(tail);
        }
    }
    return out;
}

ProjectChunkHits project_hits_finished(const ProjectScan &scan, const ProjectQuery &query,
                                       std::size_t room)
{
    ProjectChunkHits out{{}, scan};
    ProjectScan &next = out.next;
    if (!next.skipping && !next.carry.empty())
    {
        push_hit(out.hits, hit_in_line(next.carry, next.line, query), room);
        ++next.line.value;
    }
    else if (next.skipping)
    {
        ++next.line.value;
    }
    next.carry.clear();
    next.skipping = false;
    return out;
}
} // namespace nenenib::core
