#include "LexCursor.hpp"

#include "Utf8.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::core
{
// 区間の伸びのたびに割り当て直さないよう、行の長さから見積もって 1 回で取る（区間は 4 バイトに
// 1 つ程度・上限 64）。空の行は取らない。
LexCursor::LexCursor(std::string_view line) : line_(line)
{
    spans_.reserve(std::min<std::size_t>(line.size() / 4U, 64U));
}

std::string_view LexCursor::line() const noexcept
{
    return line_;
}

void LexCursor::mark(std::size_t begin, std::size_t end, SyntaxRole role)
{
    if (begin >= end)
    {
        return;
    }
    const Column first = column_at(begin);
    const Column last = column_at(end);
    spans_.push_back(ColorSpan{first, last, role});
}

void LexCursor::leave(const LexState &state) noexcept
{
    out_ = state;
}

LexLine LexCursor::finish() &&
{
    return LexLine{std::move(spans_), out_};
}

Column LexCursor::column_at(std::size_t at) noexcept
{
    column_.value += code_point_count(line_.substr(counted_, at - counted_));
    counted_ = at;
    return column_;
}
} // namespace nenenib::core
