#include "LexSql.hpp"

#include "LexCursor.hpp"
#include "LexKind.hpp"
#include "LexScan.hpp"
#include "LexState.hpp"
#include "SyntaxRole.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 字句の関数。読み始めの位置を受けて、読み終えた次の位置を返す（必ず進む）。
using LexHandler = std::size_t (*)(LexCursor &, std::size_t);

// 表は小文字で ASCII の順に並べ、大小を畳んで binary_search で引く（決定 3）。予約語は ISO SQL の
// 文と句と、PostgreSQL・MySQL・SQLite に共通の句（`limit` `offset` `returning` など）。型は
// type、集約と主な関数は呼び出しでなくても function（Vim の sqlFunction と同じ）。表どうしは
// 重ならない（`left` `right` `replace` は予約語）。
constexpr std::array<std::string_view, 112> sql_keywords{
    "add",        "all",         "alter",      "and",
    "any",        "as",          "asc",        "auto_increment",
    "begin",      "between",     "by",         "cascade",
    "case",       "cast",        "check",      "collate",
    "column",     "commit",      "constraint", "create",
    "cross",      "current",     "database",   "declare",
    "default",    "delete",      "desc",       "distinct",
    "drop",       "else",        "end",        "except",
    "exists",     "explain",     "fetch",      "first",
    "following",  "for",         "foreign",    "from",
    "full",       "function",    "grant",      "group",
    "having",     "if",          "ilike",      "in",
    "index",      "inner",       "insert",     "intersect",
    "interval",   "into",        "is",         "join",
    "key",        "lateral",     "left",       "like",
    "limit",      "merge",       "natural",    "next",
    "not",        "nulls",       "of",         "offset",
    "on",         "only",        "or",         "order",
    "outer",      "over",        "partition",  "preceding",
    "primary",    "procedure",   "range",      "recursive",
    "references", "rename",      "replace",    "returning",
    "revoke",     "right",       "rollback",   "row",
    "rows",       "schema",      "select",     "sequence",
    "set",        "table",       "temporary",  "then",
    "to",         "transaction", "trigger",    "truncate",
    "unbounded",  "union",       "unique",     "unsigned",
    "update",     "using",       "values",     "view",
    "when",       "where",       "window",     "with"};
constexpr std::array<std::string_view, 37> sql_types{
    "bigint",   "bigserial", "binary",    "bit",     "blob",     "bool",  "boolean", "bytea",
    "char",     "character", "clob",      "date",    "datetime", "dec",   "decimal", "double",
    "float",    "int",       "integer",   "json",    "longtext", "nchar", "number",  "numeric",
    "nvarchar", "precision", "real",      "serial",  "smallint", "text",  "time",    "timestamp",
    "tinyint",  "uuid",      "varbinary", "varchar", "varying"};
constexpr std::array<std::string_view, 29> sql_functions{
    "abs",        "avg",     "ceil",         "ceiling",      "coalesce",
    "concat",     "count",   "current_date", "current_time", "current_timestamp",
    "dense_rank", "extract", "floor",        "lag",          "lead",
    "length",     "lower",   "max",          "min",          "mod",
    "now",        "nullif",  "rank",         "round",        "row_number",
    "substring",  "sum",     "trim",         "upper"};
constexpr std::array<std::string_view, 3> sql_constants{"false", "null", "true"};
static_assert(std::ranges::is_sorted(sql_keywords));
static_assert(std::ranges::is_sorted(sql_types));
static_assert(std::ranges::is_sorted(sql_functions));
static_assert(std::ranges::is_sorted(sql_constants));

constexpr std::string_view operator_characters = "+-*/%=<>!|&^~(),;.:";
constexpr std::string_view quote_mark = "'";

constexpr LexState in_block_comment{
    .kind = LexKind::block_comment, .delimiter = {}, .delimiter_length = 0U};

// ---- 文字列と引用符の識別子 ----

// from から `'…'` の閉じを探し、閉じの直後を返す。`''` は閉じない（文字列の中の `'`）。`\` は
// 飛ばさない（ISO SQL）。閉じなければ lex_unclosed。
[[nodiscard]] constexpr std::size_t string_close(std::string_view line, std::size_t from) noexcept
{
    for (std::size_t at = line.find('\'', from); at != lex_unclosed; at = line.find('\'', at + 2U))
    {
        if (lex_next_of(line, at) != '\'')
        {
            return at + 1U;
        }
    }
    return lex_unclosed;
}

// `'…'` を from から塗る（at は文字列の始まり）。閉じなければ行末まで string で次の行へ。
std::size_t mark_string(LexCursor &cursor, std::size_t at, std::size_t from)
{
    const std::string_view line = cursor.line();
    const std::size_t end = string_close(line, from);
    if (end == lex_unclosed)
    {
        cursor.mark(at, line.size(), SyntaxRole::string);
        cursor.leave(lex_entered(LexKind::raw_string, quote_mark));
        return line.size();
    }
    cursor.mark(at, end, SyntaxRole::string);
    return end;
}

std::size_t lex_string(LexCursor &cursor, std::size_t at)
{
    return mark_string(cursor, at, at + 1U);
}

// 引用符の識別子 `"…"` と `` `…` ``（区間なし・中の語をキーワードにしない）。行をまたがず、
// 閉じなければ行末まで。
std::size_t skip_quoted_name(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t close = line.find(line[at], at + 1U);
    return close == lex_unclosed ? line.size() : close + 1U;
}

// ---- コメント ----

// from から `*/` を探し、`*` の位置を返す。
[[nodiscard]] constexpr std::size_t comment_close(std::string_view line, std::size_t from) noexcept
{
    for (std::size_t at = line.find('*', from); at != lex_unclosed; at = line.find('*', at + 1U))
    {
        if (lex_next_of(line, at) == '/')
        {
            return at;
        }
    }
    return lex_unclosed;
}

// `/* … */` を from から塗る（at はコメントの始まり）。閉じなければ行末まで comment で次の行へ。
// 入れ子（PostgreSQL）は数えない。
std::size_t mark_block_comment(LexCursor &cursor, std::size_t at, std::size_t from)
{
    const std::string_view line = cursor.line();
    const std::size_t close = comment_close(line, from);
    if (close == lex_unclosed)
    {
        cursor.mark(at, line.size(), SyntaxRole::comment);
        cursor.leave(in_block_comment);
        return line.size();
    }
    cursor.mark(at, close + 2U, SyntaxRole::comment);
    return close + 2U;
}

// ---- 演算子と数字 ----

[[nodiscard]] constexpr bool starts_line_comment(std::string_view line, std::size_t at) noexcept
{
    return line[at] == '-' && lex_next_of(line, at) == '-';
}

[[nodiscard]] constexpr bool starts_block_comment(std::string_view line, std::size_t at) noexcept
{
    return line[at] == '/' && lex_next_of(line, at) == '*';
}

[[nodiscard]] constexpr bool starts_number(std::string_view line, std::size_t at) noexcept
{
    return line[at] == '.' && lex_is_digit(lex_next_of(line, at));
}

// 演算子の連なりを続けるか。コメントの始まりと `.5` で切る。
[[nodiscard]] constexpr bool continues_operator(std::string_view line, std::size_t at) noexcept
{
    return operator_characters.contains(line[at]) && !starts_line_comment(line, at) &&
           !starts_block_comment(line, at) && !starts_number(line, at);
}

// 数字の途中の 1 バイトが数字を続けるか。識別子の文字（指数の `e` と 16 進の桁）、直後が桁の
// `.`、`e` の直後の符号。
[[nodiscard]] constexpr bool continues_number(std::string_view line, std::size_t at) noexcept
{
    const char value = line[at];
    if (lex_is_word(value))
    {
        return true;
    }
    if (value == '.')
    {
        return lex_is_digit(lex_next_of(line, at));
    }
    return (value == '+' || value == '-') && (line[at - 1U] | '\x20') == 'e';
}

std::size_t lex_number(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const bool hex = line[at] == '0' && (lex_next_of(line, at) | '\x20') == 'x';
    std::size_t end = at + 1U;
    while (end < line.size() && (hex ? lex_is_word(line[end]) : continues_number(line, end)))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::number);
    return end;
}

std::size_t lex_operators(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    if (starts_number(line, at))
    {
        return lex_number(cursor, at);
    }
    if (starts_line_comment(line, at))
    {
        cursor.mark(at, line.size(), SyntaxRole::comment);
        return line.size();
    }
    if (starts_block_comment(line, at))
    {
        return mark_block_comment(cursor, at, at + 2U);
    }
    std::size_t end = at + 1U;
    while (end < line.size() && continues_operator(line, end))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::operators);
    return end;
}

// ---- 識別子 ----

// 定数・予約語・型・関数の表の順に大小を畳んで引く。
[[nodiscard]] std::optional<SyntaxRole> word_role(std::string_view word) noexcept
{
    if (lex_folded_in<sql_constants>(word))
    {
        return SyntaxRole::constant;
    }
    if (lex_folded_in<sql_keywords>(word))
    {
        return SyntaxRole::keyword;
    }
    if (lex_folded_in<sql_types>(word))
    {
        return SyntaxRole::type;
    }
    return lex_folded_in<sql_functions>(word) ? std::optional{SyntaxRole::function} : std::nullopt;
}

std::size_t lex_word(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_word_end(line, at);
    const std::optional<SyntaxRole> role = word_role(line.substr(at, end - at));
    if (role.has_value())
    {
        cursor.mark(at, end, role.value());
    }
    return end;
}

std::size_t skip_character(LexCursor & /*cursor*/, std::size_t at)
{
    return at + 1U;
}

[[nodiscard]] constexpr LexHandler marked_handler(char value) noexcept
{
    constexpr std::array<std::pair<char, LexHandler>, 3> marks{
        {{'\'', lex_string}, {'"', skip_quoted_name}, {'`', skip_quoted_name}}};
    for (const auto &[mark, handler] : marks)
    {
        if (value == mark)
        {
            return handler;
        }
    }
    return operator_characters.contains(value) ? lex_operators : skip_character;
}

[[nodiscard]] constexpr LexHandler handler_of(char value) noexcept
{
    if (lex_is_digit(value))
    {
        return lex_number;
    }
    return lex_is_word(value) ? lex_word : marked_handler(value);
}

// バイト → 字句の関数の表（ADR 0156 の決定 6）。空白・`@` `$`・0x80 以上は skip_character。
constexpr auto handlers = []
{
    std::array<LexHandler, 256> table{};
    for (std::size_t index = 0; index < table.size(); ++index)
    {
        table.at(index) = handler_of(static_cast<char>(index));
    }
    return table;
}();

// ---- 行をまたぐ状態の続き ----

// 文字列の中で始まる行。`'` でない区切りは SQL の状態でないので行頭から読む。
std::size_t resume_string(LexCursor &cursor, const LexState &in)
{
    if (lex_delimiter(in) != quote_mark)
    {
        return 0U;
    }
    return mark_string(cursor, 0U, 0U);
}

std::size_t resumed(LexCursor &cursor, const LexState &in)
{
    switch (in.kind)
    {
    case LexKind::plain:
        return 0U;
    case LexKind::block_comment:
        return mark_block_comment(cursor, 0U, 0U);
    case LexKind::raw_string:
        return resume_string(cursor, in);
    }
    std::unreachable();
}
} // namespace

void lex_sql(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    for (std::size_t at = resumed(cursor, in); at < line.size();)
    {
        at = handlers.at(static_cast<unsigned char>(line[at]))(cursor, at);
    }
}
} // namespace nenenib::core
