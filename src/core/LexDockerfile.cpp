#include "LexDockerfile.hpp"

#include "LexCursor.hpp"
#include "LexKind.hpp"
#include "LexScan.hpp"
#include "LexState.hpp"
#include "SyntaxRole.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 命令（Dockerfile reference の 18 語・Vim の dockerfileKeyword と FROM）。表は小文字で ASCII の
// 順に並べ、大小を畳んで binary_search で引く（決定 3）。
constexpr std::array<std::string_view, 18> dockerfile_instructions{
    "add",    "arg",   "cmd",         "copy",  "entrypoint", "env",
    "expose", "from",  "healthcheck", "label", "maintainer", "onbuild",
    "run",    "shell", "stopsignal",  "user",  "volume",     "workdir"};
static_assert(std::ranges::is_sorted(dockerfile_instructions));

// 行末の `\` の続きの中（plain の delimiter `\`）。既定の plain は命令の頭から始まる行。
// ヒアドキュメントを開ける命令（`RUN` `COPY` `ADD`）の続きは delimiter `\<`（#861）。
constexpr LexState continued = lex_entered(LexKind::plain, "\\");
constexpr LexState heredoc_continued = lex_entered(LexKind::plain, "\\<");

// ヒアドキュメントの印の終わりと、本文の行へ渡す状態。
using Heredoc = std::pair<std::size_t, LexState>;
// 命令の頭の後ろの読み方。`FROM` の行か（`AS` を keyword にする）と、ヒアドキュメントを開けるか。
using ArgumentRules = std::pair<bool, bool>;

[[nodiscard]] constexpr std::size_t skip_spaces(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && lex_is_space(line[at]))
    {
        ++at;
    }
    return at;
}

// 行の最後の空白でない文字が `\` か（Docker の続きの判定 `\\[ \t]*$`・`# escape=` は限界）。
[[nodiscard]] constexpr bool ends_continued(std::string_view line) noexcept
{
    std::size_t end = line.size();
    while (end > 0U && lex_is_space(line[end - 1U]))
    {
        --end;
    }
    return end > 0U && line[end - 1U] == '\\';
}

// ---- ヒアドキュメント ----

// at から始まる閉じの語（引用符で囲んでもよい）の始まり・終わりと、印の終わり。語が無いか
// 引用符が閉じなければ nullopt。
[[nodiscard]] constexpr std::optional<std::array<std::size_t, 3>>
heredoc_word(std::string_view line, std::size_t at) noexcept
{
    const char quote = at < line.size() ? line[at] : '\0';
    const bool quoted = quote == '"' || quote == '\'';
    const std::size_t begin = at + (quoted ? 1U : 0U);
    const std::size_t end = lex_word_end(line, begin);
    const bool closed = !quoted || (end < line.size() && line[end] == quote);
    if (end == begin || !closed)
    {
        return std::nullopt;
    }
    return std::array<std::size_t, 3>{begin, end, end + (quoted ? 1U : 0U)};
}

// at の `<<` から始まる印 `<<EOF` `<<-EOF` `<<"EOF"` `<<'EOF'`。閉じの語（`<<-` は頭に `-`）が
// delimiter に入りきらないか、語が無いか、引用符が閉じないときは nullopt。
[[nodiscard]] constexpr std::optional<Heredoc> heredoc_marker(std::string_view line,
                                                              std::size_t at) noexcept
{
    const bool dashed = lex_next_of(line, at + 1U) == '-';
    const std::optional<std::array<std::size_t, 3>> word =
        lex_next_of(line, at) == '<' ? heredoc_word(line, at + (dashed ? 3U : 2U)) : std::nullopt;
    if (!word.has_value())
    {
        return std::nullopt;
    }
    const auto [begin, end, after] = word.value();
    const std::size_t dash = dashed ? 1U : 0U;
    if (dash + end - begin > raw_delimiter_limit)
    {
        return std::nullopt;
    }
    LexState state = lex_entered(LexKind::raw_string, std::string_view{"-"}.substr(0U, dash));
    std::ranges::copy(line.substr(begin, end - begin),
                      std::next(state.delimiter.begin(), dashed ? 1 : 0));
    state.delimiter_length = static_cast<std::uint8_t>(dash + end - begin);
    return Heredoc{after, state};
}

// ヒアドキュメントの本文の行。閉じの語だけの行（`<<-` は行頭のタブを除いて）で plain に戻る。
// 本文と閉じの行は全体が string（中のシェルやファイルの中身は分類しない）。
void lex_heredoc_body(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    const std::string_view delimiter = lex_delimiter(in);
    const bool dashed = delimiter.starts_with('-');
    std::size_t first = 0U;
    while (dashed && first < line.size() && line[first] == '\t')
    {
        ++first;
    }
    cursor.mark(0U, line.size(), SyntaxRole::string);
    const bool closing = line.substr(first) == delimiter.substr(dashed ? 1U : 0U);
    cursor.leave(closing ? LexState{} : in);
}

// ---- 字句 ----

// `"…"`（`\` は次の 1 バイトを飛ばす）。閉じなければ行末まで string（行をまたがない）。
[[nodiscard]] std::size_t lex_string(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_closing_quote(line, at);
    const std::size_t stop = end == lex_unclosed ? line.size() : end;
    cursor.mark(at, stop, SyntaxRole::string);
    return stop;
}

// `${…}`（閉じの `}` まで・閉じなければ行末まで）と `$NAME`。どちらでもない `$` は 1 字だけ進む。
[[nodiscard]] std::size_t lex_variable(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const char next = lex_next_of(line, at);
    std::size_t end = at + 1U;
    if (next == '{')
    {
        const std::size_t close = line.find('}', at + 2U);
        end = close == lex_unclosed ? line.size() : close + 1U;
    }
    else if (lex_is_word(next) && !lex_is_digit(next))
    {
        end = lex_word_end(line, at + 1U);
    }
    if (end > at + 1U)
    {
        cursor.mark(at, end, SyntaxRole::constant);
    }
    return end;
}

// BuildKit のヒアドキュメントを開ける命令か（`RUN` `COPY` `ADD`・#861）。
[[nodiscard]] constexpr bool opens_heredocs(std::string_view instruction) noexcept
{
    return lex_folded_equal(instruction, "run") || lex_folded_equal(instruction, "copy") ||
           lex_folded_equal(instruction, "add");
}

// 命令の頭の語（`ONBUILD` の後ろの語も命令）。命令の表にあれば keyword。読み終えた位置と、
// 後ろを読む決まりを返す。
[[nodiscard]] std::pair<std::size_t, ArgumentRules> lex_instruction(LexCursor &cursor,
                                                                    std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t end = lex_word_end(line, at);
    std::string_view word = line.substr(at, end - at);
    if (!lex_folded_in(dockerfile_instructions, word))
    {
        return {end, ArgumentRules{false, false}};
    }
    cursor.mark(at, end, SyntaxRole::keyword);
    if (lex_folded_equal(word, "onbuild"))
    {
        const std::size_t next = skip_spaces(line, end);
        const std::size_t after = lex_word_end(line, next);
        word = line.substr(next, after - next);
        if (lex_folded_in(dockerfile_instructions, word))
        {
            cursor.mark(next, after, SyntaxRole::keyword);
            end = after;
        }
    }
    return {end, ArgumentRules{lex_folded_equal(word, "from"), opens_heredocs(word)}};
}

// `<`。ヒアドキュメントを開ける命令で、直前が空白か行頭の行の最初の印なら string に塗り、本文へ
// 渡す状態を heredoc に覚える（2 つ目からは読まない・限界）。`$((1<<4))` `a<<b`
// は開かない（#861）。
[[nodiscard]] std::size_t lex_less(LexCursor &cursor, std::size_t at, bool heredocs,
                                   std::optional<LexState> &heredoc)
{
    const std::string_view line = cursor.line();
    const bool spaced = at == 0U || lex_is_space(line[at - 1U]);
    const std::optional<Heredoc> marker =
        heredocs && spaced && !heredoc.has_value() ? heredoc_marker(line, at) : std::nullopt;
    if (!marker.has_value())
    {
        return at + 1U;
    }
    cursor.mark(at, marker.value().first, SyntaxRole::string);
    heredoc = marker.value().second;
    return marker.value().first;
}

// 語。`FROM` の行の `AS` だけ keyword（ほかの語は区間なし）。
[[nodiscard]] std::size_t lex_argument_word(LexCursor &cursor, std::size_t at, bool from)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_word_end(line, at);
    if (from && lex_folded_equal(line.substr(at, end - at), "as"))
    {
        cursor.mark(at, end, SyntaxRole::keyword);
    }
    return end;
}

// 命令の頭の後ろの 1 字句。`\` は次の 1 バイトを飛ばす（行末の続きは呼び手が見る）。
[[nodiscard]] std::size_t lex_argument(LexCursor &cursor, std::size_t at, ArgumentRules rules,
                                       std::optional<LexState> &heredoc)
{
    const char value = cursor.line()[at];
    if (value == '<')
    {
        return lex_less(cursor, at, rules.second, heredoc);
    }
    if (value == '"')
    {
        return lex_string(cursor, at);
    }
    if (value == '$')
    {
        return lex_variable(cursor, at);
    }
    if (lex_is_word(value))
    {
        return lex_argument_word(cursor, at, rules.first);
    }
    return at + (value == '\\' ? 2U : 1U);
}

// 命令の頭の後ろ（続きの行は行頭から）を読む。ヒアドキュメントの印があれば、その本文へ渡す
// 状態を返す。
[[nodiscard]] std::optional<LexState> lex_arguments(LexCursor &cursor, std::size_t at,
                                                    ArgumentRules rules)
{
    std::optional<LexState> heredoc;
    while (at < cursor.line().size())
    {
        at = lex_argument(cursor, at, rules, heredoc);
    }
    return heredoc;
}
} // namespace

void lex_dockerfile(LexCursor &cursor, const LexState &in)
{
    if (in.kind == LexKind::raw_string)
    {
        lex_heredoc_body(cursor, in);
        return;
    }
    const std::string_view line = cursor.line();
    const bool continuation = in == continued || in == heredoc_continued;
    const LexState kept = continuation ? in : LexState{};
    const std::size_t first = lex_first_visible(line);
    if (first == line.size() || line[first] == '#')
    {
        cursor.mark(first, line.size(), SyntaxRole::comment);
        cursor.leave(kept);
        return;
    }
    const auto [at, rules] = continuation
                                 ? std::pair{first, ArgumentRules{false, in == heredoc_continued}}
                                 : lex_instruction(cursor, first);
    const std::optional<LexState> heredoc = lex_arguments(cursor, at, rules);
    if (heredoc.has_value())
    {
        cursor.leave(heredoc.value());
        return;
    }
    const LexState next = rules.second ? heredoc_continued : continued;
    cursor.leave(ends_continued(line) ? next : LexState{});
}
} // namespace nenenib::core
