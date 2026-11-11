#include "MinimapCache.hpp"

#include "Lexer.hpp"
#include "Minimap.hpp"
#include "Offset.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::application
{
namespace
{
// 行 line（1 始まり）の本文。core::minimap_line_bytes を超える行は先頭だけ（code point の境で
// 切る）で、long に真を置く。
[[nodiscard]] std::string line_body(const core::TextBuffer &text, core::LineNumber line,
                                    bool &long_line)
{
    const core::Offset start = text.line_start(line);
    const core::Offset end = text.line_end(line);
    long_line = end.value - start.value > core::minimap_line_bytes;
    if (!long_line)
    {
        return text.text_range(start, end);
    }
    std::string head = text.text_range(start, core::Offset{start.value + core::minimap_line_bytes});
    head.resize(core::whole_code_points(head));
    return head;
}

// 窓の最初の行 first の行頭の状態（core::syntax_backfill_lines 行の遡り・覚えは使わない）。
[[nodiscard]] core::LexState state_before(const core::TextBuffer &text, core::Language language,
                                          std::size_t first, std::size_t &lexed)
{
    const std::size_t start = first - std::min(first - 1, core::syntax_backfill_lines(language));
    core::LexState state = core::lex_start_state(language, start == 1);
    for (std::size_t line = start; line < first; ++line)
    {
        bool long_line = false;
        const std::string body = line_body(text, core::LineNumber{line}, long_line);
        state = long_line ? core::LexState{} : core::lex_state(language, state, body);
        ++lexed;
    }
    return state;
}
} // namespace

bool MinimapCache::holds(const MinimapSource &source,
                         const core::MinimapWindow &window) const noexcept
{
    const auto held = window_of(source);
    return held.has_value() && held.value() == window;
}

std::optional<core::MinimapWindow>
MinimapCache::window_of(const MinimapSource &source) const noexcept
{
    if (rows_ == nullptr || !(source_ == source))
    {
        return std::nullopt;
    }
    return rows_->window;
}

// 長い行は色を付けず、状態を plain に戻す（巨大な行の窓と同じ限界・ADR 0156）。
core::MinimapRow MinimapCache::row_at(const core::TextBuffer &text, core::LineNumber line,
                                      core::LexState &state)
{
    bool long_line = false;
    const std::string body = line_body(text, line, long_line);
    if (long_line)
    {
        state = core::LexState{};
        return core::minimap_row(body, {});
    }
    if (!source_.lexing)
    {
        return core::minimap_row(body, {});
    }
    ++lexed_;
    auto result = core::lex_line(source_.language, state, body);
    state = result.out;
    return core::minimap_row(body, result.spans);
}

void MinimapCache::rebuild(const core::TextBuffer &text, const MinimapSource &source,
                           const core::MinimapWindow &window)
{
    source_ = source;
    std::vector<core::MinimapRow> rows;
    rows.reserve(window.rows);
    states_.clear();
    states_.reserve(window.rows + 1);
    core::LexState state = source.lexing
                               ? state_before(text, source.language, window.first.value, lexed_)
                               : core::LexState{};
    for (std::size_t index = 0; index < window.rows; ++index)
    {
        states_.push_back(state);
        rows.push_back(row_at(text, core::LineNumber{window.first.value + index}, state));
    }
    states_.push_back(state);
    ++serial_;
    rows_ = std::make_shared<MinimapRows>(MinimapRows{window, std::move(rows), serial_});
    base_ = 0;
    dirty_.reset();
}

void MinimapCache::edited(const core::TextBuffer &before, const core::TextBuffer &after,
                          const core::Edit &edit, AutosaveId tab)
{
    if (rows_ == nullptr || !(source_.tab == tab))
    {
        return;
    }
    const std::size_t first = rows_->window.first.value;
    const std::size_t count = rows_->rows.size();
    const std::size_t line = before.position_of(edit.at).line.value;
    const auto removed = static_cast<std::size_t>(std::ranges::count(edit.removed, '\n'));
    const auto inserted = static_cast<std::size_t>(std::ranges::count(edit.inserted, '\n'));
    if (line >= first + count)
    {
        return;
    }
    // 窓より上の編集は窓の行の番号と字句の状態を変え、窓の終わりをまたぐ削除は窓の外の行を窓へ
    // 引き込む。どちらも次の frame で窓ごと作り直す（限界）。
    if (line < first || line + removed >= first + count)
    {
        forget();
        return;
    }
    relex_band(after, line - first, removed + 1, inserted + 1);
}

void MinimapCache::relex_band(const core::TextBuffer &after, std::size_t begin, std::size_t removed,
                              std::size_t inserted)
{
    auto &rows = rows_->rows;
    const std::size_t first = rows_->window.first.value;
    core::LexState state = states_.at(begin);
    if (removed == inserted)
    {
        // 行の増減の無い編集（いちばん多い打鍵）は、変わった行の要素だけをその場で書き換える
        // （窓の 4,000 要素を動かさない・レビュー L1）。
        for (std::size_t index = begin; index < begin + inserted; ++index)
        {
            states_.at(index) = state;
            rows.at(index) = row_at(after, core::LineNumber{first + index}, state);
        }
    }
    else
    {
        replace_rows(after, core::MinimapBand{begin, removed, inserted}, state);
    }
    // 帯の下は、行頭の状態が前と揃うまで読み直す（ブロックコメントを開いた・閉じた編集）。
    std::size_t end = begin + inserted;
    while (end < rows.size() && !(states_.at(end) == state))
    {
        states_.at(end) = state;
        rows.at(end) = row_at(after, core::LineNumber{first + end}, state);
        ++end;
    }
    if (end == rows.size())
    {
        states_.back() = state;
    }
    const std::size_t walked = end - begin - inserted;
    rows_->window.rows = rows.size();
    ++serial_;
    rows_->serial = serial_;
    const core::MinimapBand band{begin, removed + walked, inserted + walked};
    dirty_ = dirty_.has_value() ? core::minimap_band_joined(dirty_.value(), band) : band;
}

void MinimapCache::replace_rows(const core::TextBuffer &after, const core::MinimapBand &band,
                                core::LexState &state)
{
    const auto [begin, removed, inserted] = band;
    auto &rows = rows_->rows;
    const std::size_t first = rows_->window.first.value;
    std::vector<core::MinimapRow> fresh;
    std::vector<core::LexState> starts;
    fresh.reserve(inserted);
    starts.reserve(inserted);
    for (std::size_t index = 0; index < inserted; ++index)
    {
        starts.push_back(state);
        fresh.push_back(row_at(after, core::LineNumber{first + begin + index}, state));
    }
    const auto at = static_cast<std::ptrdiff_t>(begin);
    const auto span = static_cast<std::ptrdiff_t>(removed);
    rows.erase(rows.begin() + at, rows.begin() + at + span);
    rows.insert(rows.begin() + at, std::make_move_iterator(fresh.begin()),
                std::make_move_iterator(fresh.end()));
    states_.erase(states_.begin() + at, states_.begin() + at + span);
    states_.insert(states_.begin() + at, starts.begin(), starts.end());
}

void MinimapCache::forget() noexcept
{
    rows_.reset();
    states_.clear();
    dirty_.reset();
    base_ = 0;
}

MinimapFrame MinimapCache::framed(core::LineNumber top, core::LineNumber first_visible,
                                  std::size_t visible)
{
    MinimapFrame frame{rows_, top, first_visible, visible, base_, dirty_};
    base_ = rows_ == nullptr ? 0 : rows_->serial;
    dirty_.reset();
    return frame;
}

std::size_t MinimapCache::lexed() const noexcept
{
    return lexed_;
}
} // namespace nenenib::application
