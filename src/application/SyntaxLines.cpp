#include "SyntaxLines.hpp"

#include "GiantLineWindow.hpp"
#include "Lexer.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace nenenib::application
{
namespace
{
// 覚えから続けてよいか（決定 9・追補 #827）。同じ文書を同じ言語で、見えている行の手前から。覚えの
// 行より前の編集は controller が覚えを捨てているので、改訂番号は比べない。
[[nodiscard]] bool continues(const SyntaxStateCache &key, const SyntaxStateCache &cache) noexcept
{
    return cache.tab == key.tab && cache.language == key.language &&
           cache.line.value <= key.line.value &&
           key.line.value - cache.line.value <= syntax_cache_reach;
}

// 遡りの 1 行。巨大な行は行の全体を写さず plain に戻す（ADR 0139 の窓と同じく行を読まない・限界）。
[[nodiscard]] core::LexState passed(const core::TextBuffer &text, core::Language language,
                                    const core::LexState &in, core::LineNumber line)
{
    if (giant_line_index(text, line) != nullptr)
    {
        return core::LexState{};
    }
    const std::string body = text.text_range(text.line_start(line), text.line_end(line));
    return core::lex_state(language, in, body);
}

// IME の差し込み行の区間（#839）。差し込みの手前は本文の桁のまま、後ろは差し込んだ code point の
// 数だけずらし、差し込みをまたぐ区間は 2 つに割る（格子の split_colors と同じ理屈）。
[[nodiscard]] std::vector<core::ColorSpan>
composed_colors(const std::vector<core::ColorSpan> &spans, core::Column at, std::size_t count)
{
    std::vector<core::ColorSpan> result;
    result.reserve(spans.size() + 1);
    const auto moved = [count](core::Column column) { return core::Column{column.value + count}; };
    for (const auto &span : spans)
    {
        if (span.end.value <= at.value)
        {
            result.push_back(span);
            continue;
        }
        if (span.begin.value < at.value)
        {
            result.push_back(core::ColorSpan{span.begin, at, span.role});
            result.push_back(core::ColorSpan{moved(at), moved(span.end), span.role});
            continue;
        }
        result.push_back(core::ColorSpan{moved(span.begin), moved(span.end), span.role});
    }
    return result;
}
} // namespace

std::optional<core::LexState> syntax_begin(const core::TextBuffer &text,
                                           const SyntaxStateCache &key,
                                           std::optional<SyntaxStateCache> &cache,
                                           std::size_t &lexed)
{
    if (!core::has_lexer(key.language) || key.line.value == 0)
    {
        return std::nullopt;
    }
    const bool kept = cache.has_value() && continues(key, cache.value());
    std::size_t line = kept ? cache.value().line.value
                            : key.line.value - std::min(key.line.value - 1,
                                                        core::syntax_backfill_lines(key.language));
    core::LexState state = kept ? cache.value().state : core::LexState{};
    for (; line < key.line.value; ++line)
    {
        state = passed(text, key.language, state, core::LineNumber{line});
        ++lexed;
    }
    cache = SyntaxStateCache{key.tab, key.language, key.line, state};
    return state;
}

std::optional<core::LexState> colored_line(core::Language language,
                                           const std::optional<core::LexState> &in, LineView &view,
                                           std::size_t &lexed)
{
    if (!in.has_value())
    {
        return std::nullopt;
    }
    ++lexed;
    if (view.window.active)
    {
        return core::LexState{};
    }
    auto result = core::lex_line(language, in.value(), view.text);
    view.colors = view.composed.has_value()
                      ? composed_colors(result.spans,
                                        composed_source_column(view.display, view.composed.value()),
                                        view.composed.value().inserted_columns)
                      : std::move(result.spans);
    return result.out;
}

std::optional<core::LexState> grid_colored_line(const EditorState &state,
                                                const std::optional<core::LexState> &in,
                                                core::LineNumber line,
                                                std::vector<GridColorRange> &colors)
{
    if (!in.has_value())
    {
        return std::nullopt;
    }
    const auto &text = state.text();
    if (giant_line_index(text, line) != nullptr)
    {
        return core::LexState{};
    }
    const auto start = text.line_start(line);
    const std::string body = text.text_range(start, text.line_end(line));
    auto result = core::lex_line(state.language(), in.value(), body);
    // 区間は昇順なので、桁と位置を 1 回だけ前へ歩いて写す。
    std::size_t byte = 0;
    std::size_t column = 1;
    const auto offset_of = [&byte, &column, &body, start](core::Column wanted)
    {
        for (; column < wanted.value && byte < body.size(); ++column)
        {
            byte = core::next_code_point(body, core::Offset{byte}).value;
        }
        return core::Offset{start.value + byte};
    };
    for (const auto &span : result.spans)
    {
        const auto begin = offset_of(span.begin);
        colors.push_back(GridColorRange{core::OffsetRange{begin, offset_of(span.end)}, span.role});
    }
    return result.out;
}
} // namespace nenenib::application
