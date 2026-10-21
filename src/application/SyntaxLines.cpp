#include "SyntaxLines.hpp"

#include "GiantLineWindow.hpp"
#include "Lexer.hpp"

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
    if (view.composed.has_value())
    {
        return core::lex_state(language, in.value(), view.text);
    }
    auto result = core::lex_line(language, in.value(), view.text);
    view.colors = std::move(result.spans);
    return result.out;
}
} // namespace nenenib::application
