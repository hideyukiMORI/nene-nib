#include "MarkdownBuilder.hpp"

#include "MarkdownAlert.hpp"
#include "MarkdownImage.hpp"
#include "MarkdownTask.hpp"
#include "Offset.hpp"
#include "PreviewSpan.hpp"
#include "PreviewTableRow.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::string_view bullet_marker = "•";
constexpr std::size_t depth_limit = 255U;
// 画像の代わりの文字の枠（#784）。alt は表の文 preview_image_alt の置き場 {0} の位置に入る
// （ADR 0123 の追補 2026-11-13）。置き場の前が開き、後ろが閉じ。
[[nodiscard]] std::string_view image_opening_of(DisplayLanguage language) noexcept
{
    return first_slot_split(ui_text(UiMessage::preview_image_alt, language)).first;
}

[[nodiscard]] std::string_view image_closing_of(DisplayLanguage language) noexcept
{
    return first_slot_split(ui_text(UiMessage::preview_image_alt, language)).second;
}

[[nodiscard]] std::uint8_t saturated_depth(std::size_t depth) noexcept
{
    return static_cast<std::uint8_t>(depth < depth_limit ? depth : depth_limit);
}

// md4c と同じく CR と LF のどちらも改行（ISNEWLINE_・#887）。
[[nodiscard]] bool newline(char value) noexcept
{
    return value == '\r' || value == '\n';
}

// from から次の改行の位置（無ければ input.size()）。1 バイトずつ歩く（STL の検索は Debug で
// memchr を core の外へ出す・ARC-003・#784）。
[[nodiscard]] std::size_t line_end(std::string_view input, std::size_t from) noexcept
{
    while (from < input.size() && !newline(input[from]))
    {
        ++from;
    }
    return from;
}

// 改行の位置 end の次の行の行頭。CRLF は 1 つの改行（#887）。
[[nodiscard]] std::size_t next_line(std::string_view input, std::size_t end) noexcept
{
    const bool crlf = input[end] == '\r' && end + 1U < input.size() && input[end + 1U] == '\n';
    return end + (crlf ? 2U : 1U);
}

[[nodiscard]] bool blank(char value) noexcept
{
    return value == ' ' || value == '\t';
}

// [from, to) が空白（空白・Tab）だけか。
[[nodiscard]] bool blank_between(std::string_view input, std::size_t from, std::size_t to) noexcept
{
    for (; from < to; ++from)
    {
        if (!blank(input[from]))
        {
            return false;
        }
    }
    return true;
}

// line から始めて、空白（空白・Tab）だけの行を飛ばした最初の行の行頭。
[[nodiscard]] std::size_t first_filled_line(std::string_view input, std::size_t line) noexcept
{
    while (line < input.size())
    {
        const auto end = line_end(input, line);
        if (end == input.size() || !blank_between(input, line, end))
        {
            return line;
        }
        line = next_line(input, end);
    }
    return line;
}

constexpr std::string_view replacement = "\xEF\xBF\xBD";

// [from, to) の改行を LF にそろえる（CRLF と CR だけの改行も LF・md4c のコードブロックと同じ形）。
[[nodiscard]] std::string lf_text(std::string_view input, std::size_t from, std::size_t to)
{
    std::string text;
    text.reserve(to - from);
    for (; from < to; ++from)
    {
        const bool crlf = input[from] == '\r' && from + 1U < to && input[from + 1U] == '\n';
        if (crlf)
        {
            continue;
        }
        // NUL は本文と同じく U+FFFD（md4c の MD_TEXT_NULLCHAR と同じ・#1840 のレビュー）。
        if (input[from] == '\0')
        {
            text.append(replacement);
            continue;
        }
        text.push_back(input[from] == '\r' ? '\n' : input[from]);
    }
    return text;
}

// line から始まる行の中身（改行と後ろの空白・Tab を除く）が word だけか（Jekyll と同じ・#1840 の
// レビュー）。
[[nodiscard]] bool line_is(std::string_view input, std::size_t line, std::string_view word) noexcept
{
    auto end = line_end(input, line);
    while (end > line && blank(input[end - 1U]))
    {
        --end;
    }
    return input.substr(line, end - line) == word;
}

// 打ち消しの開く記号の `~` の数。at は span の中の最初の文字の本文の位置で、その前の入れ子の
// 開く記号（強調・コード・リンク・画像・自動リンク・エスケープ）を飛ばして `~` の並びを数える。
// 読めない形は 2（打ち消しのまま）。
[[nodiscard]] std::size_t opening_tildes(std::string_view input, std::size_t at) noexcept
{
    constexpr std::string_view openers = "*_`[!<\\";
    while (at > 0U && openers.contains(input[at - 1U]))
    {
        --at;
    }
    std::size_t count = 0U;
    while (at > 0U && input[at - 1U] == '~')
    {
        --at;
        ++count;
    }
    return count == 0U ? 2U : count;
}

// line から始めて、`---` か `...` だけの最初の行の行頭
// （front matter の閉じる行・無ければ nullopt）。
[[nodiscard]] std::optional<std::size_t> closing_line(std::string_view input,
                                                      std::size_t line) noexcept
{
    while (line < input.size())
    {
        if (line_is(input, line, "---") || line_is(input, line, "..."))
        {
            return line;
        }
        const auto end = line_end(input, line);
        line = end == input.size() ? end : next_line(input, end);
    }
    return std::nullopt;
}
} // namespace

MarkdownBuilder::MarkdownBuilder(std::string_view input, const MarkdownStop &stop,
                                 DisplayLanguage language) noexcept
    : input_(input), stop_(&stop), image_opening_(image_opening_of(language)),
      image_closing_(image_closing_of(language))
{
}

bool MarkdownBuilder::should_stop()
{
    stopped_ = stopped_ || stop_->should_stop();
    return stopped_;
}

bool MarkdownBuilder::stopped() const noexcept
{
    return stopped_;
}

void MarkdownBuilder::exhaust() noexcept
{
    exhausted_ = true;
}

bool MarkdownBuilder::exhausted() const noexcept
{
    return exhausted_;
}

std::size_t MarkdownBuilder::front_matter()
{
    const auto opening = line_end(input_, 0U);
    if (!line_is(input_, 0U, "---") || opening == input_.size())
    {
        return 0U;
    }
    const auto content = next_line(input_, opening);
    const auto closing = closing_line(input_, content);
    if (!closing.has_value())
    {
        return 0U;
    }
    auto block = started(PreviewBlockKind::front_matter, 0U);
    block.text = lf_text(input_, content, closing.value());
    blocks_.push_back(std::move(block));
    // 位置を持たない次の要素（水平線など）は、閉じる行の次の行から探す。
    last_end_ = closing.value();
    const auto end = line_end(input_, closing.value());
    parsed_from_ = end == input_.size() ? end : next_line(input_, end);
    return parsed_from_;
}

void MarkdownBuilder::enter_quote()
{
    close_implicit();
    // alert はいちばん外の引用（リストの外）だけ（GitHub と同じ・#1848）。
    if (quote_depth_ == 0U && lists_.empty())
    {
        alert_start_ = blocks_.size();
        alert_.reset();
    }
    ++quote_depth_;
}

void MarkdownBuilder::leave_quote()
{
    close_implicit();
    quote_depth_ -= quote_depth_ == 0U ? 0U : 1U;
    if (quote_depth_ == 0U)
    {
        alert_start_.reset();
        alert_.reset();
    }
}

void MarkdownBuilder::mark_alert(PreviewBlock &block)
{
    if (!alert_start_.has_value() || quote_depth_ == 0U)
    {
        return;
    }
    const bool first = blocks_.size() == alert_start_.value();
    if (first && quote_depth_ == 1U && block.kind == PreviewBlockKind::paragraph)
    {
        alert_ = titled_alert(block);
    }
    block.alert = alert_;
}

std::optional<PreviewAlert> MarkdownBuilder::titled_alert(PreviewBlock &block)
{
    if (!leaf_begin_.has_value())
    {
        return std::nullopt;
    }
    const auto begin = leaf_begin_.value();
    const auto kind = markdown_alert_line(input_.substr(begin, line_end(input_, begin) - begin));
    const auto shift = kind.has_value() ? markdown_alert_titled(block, kind.value())
                                        : std::optional<std::ptrdiff_t>{};
    if (!shift.has_value())
    {
        return std::nullopt;
    }
    // この要素の画像の節も同じだけずらす。
    const auto moved = [&shift](Offset offset)
    {
        return Offset{
            static_cast<std::size_t>(static_cast<std::ptrdiff_t>(offset.value) + shift.value())};
    };
    for (auto &image : images_)
    {
        if (image.block == blocks_.size())
        {
            image.begin = moved(image.begin);
            image.end = moved(image.end);
        }
    }
    return kind;
}

void MarkdownBuilder::enter_table()
{
    close_implicit();
    flush_marker();
    table_row_ = 0U;
}

void MarkdownBuilder::enter_row()
{
    auto &row = opened(PreviewBlockKind::table_row, 0U);
    row.table = PreviewTableRow{.row = table_row_, .header = false, .cells = {}, .aligns = {}};
    ++table_row_;
}

void MarkdownBuilder::enter_cell(PreviewTableAlign align, bool header)
{
    auto &row = ensure_leaf();
    if (!row.table.has_value())
    {
        return;
    }
    auto &table = row.table.value();
    if (!table.cells.empty())
    {
        row.text.push_back('\t');
    }
    table.cells.push_back(Offset{row.text.size()});
    table.aligns.push_back(align);
    table.header = table.header || header;
}

void MarkdownBuilder::enter_list(std::optional<std::pair<std::uint64_t, char>> numbering)
{
    // 記号だけの項目の直後に入れ子のリストが来たら（"- - a"）、外の項目の記号を空の要素で出す。
    close_implicit();
    flush_marker();
    lists_.push_back(numbering);
}

void MarkdownBuilder::leave_list()
{
    close_implicit();
    if (!lists_.empty())
    {
        lists_.pop_back();
    }
}

void MarkdownBuilder::enter_item(std::optional<MarkdownTask> task)
{
    close_implicit();
    flush_marker();
    task_ = task;
    if (task_.has_value())
    {
        task_.value().mark_offset += parsed_from_;
    }
    const auto numbering = lists_.empty() ? std::nullopt : lists_.back();
    if (!numbering.has_value())
    {
        marker_ = std::string(bullet_marker);
        return;
    }
    const auto [number, delimiter] = numbering.value();
    marker_ = std::to_string(number) + delimiter;
    lists_.back() = std::pair{number + 1U, delimiter};
}

void MarkdownBuilder::leave_item()
{
    close_implicit();
    flush_marker();
}

void MarkdownBuilder::rule()
{
    close_implicit();
    open_leaf(PreviewBlockKind::rule, 0U);
    close_leaf();
}

void MarkdownBuilder::open_leaf(PreviewBlockKind kind, std::uint8_t level)
{
    static_cast<void>(opened(kind, level));
}

void MarkdownBuilder::close_leaf()
{
    if (!leaf_.has_value())
    {
        return;
    }
    auto block = std::move(leaf_).value();
    leaf_.reset();
    // md4c は装飾を要素の中で閉じるので、ここで開いている画像の節は残らない。残れば今の終わりで閉じ
    // る（閉じの「]」は足さない）。
    while (!open_images_.empty())
    {
        finish_image(block, false);
    }
    // 行の探索は位置を持たない要素のときだけ走らせる（value_or は先に評価してしまう）。
    if (leaf_begin_.has_value())
    {
        block.source_begin = Offset{leaf_begin_.value()};
    }
    else
    {
        block.source_begin = Offset{unlocated_begin()};
    }
    mark_alert(block);
    // `~` 1 つの打ち消しは装飾を外す（番号の大きい方から）。
    std::ranges::sort(single_strikes_, std::greater{});
    for (const auto span : single_strikes_)
    {
        block.spans.erase(block.spans.begin() + static_cast<std::ptrdiff_t>(span));
    }
    single_strikes_.clear();
    pending_strikes_.clear();
    blocks_.push_back(std::move(block));
    leaf_begin_.reset();
    implicit_leaf_ = false;
    spans_.clear();
}

void MarkdownBuilder::enter_span(std::optional<PreviewStyle> style)
{
    auto &block = ensure_leaf();
    if (!style.has_value())
    {
        spans_.emplace_back(std::nullopt);
        return;
    }
    const Offset at{block.text.size()};
    spans_.emplace_back(block.spans.size());
    block.spans.push_back(PreviewSpan{at, at, style.value()});
    if (style.value() == PreviewStyle::image)
    {
        block.text.append(image_opening_);
    }
}

void MarkdownBuilder::enter_strike()
{
    enter_span(PreviewStyle::strikethrough);
    const auto &block = ensure_leaf();
    pending_strikes_.emplace_back(block.spans.size() - 1U, block.text.size());
}

void MarkdownBuilder::settle_strikes(std::optional<std::size_t> at)
{
    for (const auto &[span, begin] : pending_strikes_)
    {
        if (at.has_value() && opening_tildes(input_, at.value()) == 1U)
        {
            restore_opening(span, begin);
        }
    }
    pending_strikes_.clear();
}

void MarkdownBuilder::restore_opening(std::size_t span, std::size_t begin)
{
    // 開く `~` を始まりへ戻し、打ち消しの後に開いた装飾（入れ子）と画像の節を 1 つずらす。
    auto &block = ensure_leaf();
    block.text.insert(begin, 1U, '~');
    for (std::size_t other = span + 1U; other < block.spans.size(); ++other)
    {
        auto &moved = block.spans.at(other);
        if (moved.begin.value >= begin)
        {
            moved.begin = Offset{moved.begin.value + 1U};
            moved.end = Offset{std::max(moved.end.value + 1U, moved.begin.value)};
        }
    }
    for (const auto image : open_images_)
    {
        auto &node = images_.at(image);
        node.begin = Offset{node.begin.value + (node.begin.value >= begin ? 1U : 0U)};
    }
    single_strikes_.push_back(span);
}

void MarkdownBuilder::leave_span()
{
    if (spans_.empty())
    {
        return;
    }
    const auto index = spans_.back();
    spans_.pop_back();
    if (index.has_value() && leaf_.has_value())
    {
        std::erase_if(pending_strikes_,
                      [&index](const auto &pending) { return pending.first == index.value(); });
        auto &block = leaf_.value();
        auto &span = block.spans.at(index.value());
        const bool image = span.style == PreviewStyle::image;
        if (image)
        {
            block.text.append(image_closing_);
        }
        // `~` 1 つの打ち消しは閉じる `~` も文字に戻す。
        if (std::ranges::contains(single_strikes_, index.value()))
        {
            block.text.push_back('~');
        }
        span.end = Offset{block.text.size()};
        if (image)
        {
            finish_image(block, true);
        }
    }
}

void MarkdownBuilder::enter_image(std::string source, std::string title)
{
    enter_span(PreviewStyle::image);
    // 要素は開いたまま閉じるまで blocks_ に入らないので、閉じたときの番号は今の blocks_ の数。
    const Offset begin = ensure_leaf().spans.back().begin;
    open_images_.push_back(images_.size());
    images_.push_back(
        MarkdownImage{blocks_.size(), begin, begin, std::move(source), std::move(title), {}});
}

void MarkdownBuilder::finish_image(const PreviewBlock &block, bool closed)
{
    auto &image = images_.at(open_images_.back());
    open_images_.pop_back();
    image.end = Offset{block.text.size()};
    const std::size_t from = image.begin.value + image_opening_.size();
    const std::size_t to = image.end.value - (closed ? image_closing_.size() : 0U);
    image.alt = to > from ? block.text.substr(from, to - from) : std::string{};
}

void MarkdownBuilder::text(std::string_view shown, std::string_view source)
{
    auto &leaf = ensure_leaf();
    const auto from = leaf.text.size();
    leaf.text.append(shown);
    // 表の行の Tab は升の間だけ（升の中の Tab は空白にする・#1848）。
    if (leaf.kind == PreviewBlockKind::table_row)
    {
        std::ranges::replace(leaf.text.begin() + static_cast<std::ptrdiff_t>(from), leaf.text.end(),
                             '\t', ' ');
    }
    const auto at = located(source);
    if (!pending_strikes_.empty())
    {
        settle_strikes(at);
    }
    if (!at.has_value())
    {
        return;
    }
    if (!leaf_begin_.has_value())
    {
        leaf_begin_ = at;
    }
    last_end_ = at.value() + source.size();
}

MarkdownDocument MarkdownBuilder::finished() &&
{
    close_implicit();
    flush_marker();
    close_leaf();
    return MarkdownDocument{std::move(blocks_), std::move(images_)};
}

std::optional<std::size_t> MarkdownBuilder::located(std::string_view source) const
{
    // md4c の固定の文字列（改行・字下げ）は本文の外にある。別の配列の pointer の大小は
    // std::less_equal の全順序で比べる。
    const std::less_equal<const char *> not_after;
    if (source.empty() || !not_after(input_.data(), source.data()) ||
        !not_after(source.data() + source.size(), input_.data() + input_.size()))
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(source.data() - input_.data());
}

// 文字の位置を持たない要素（水平線・空の項目・空のコードブロック）は、直前の要素の終わりの
// 次の行から空行を飛ばした最初の行の行頭（ADR 0151 の決定 8 の追記）。前に位置が無ければ
// 本文の先頭から空行を飛ばす。
std::size_t MarkdownBuilder::unlocated_begin() const noexcept
{
    std::size_t line = 0U;
    if (last_end_.has_value())
    {
        const auto end = line_end(input_, last_end_.value());
        line = end == input_.size() ? input_.size() : next_line(input_, end);
    }
    return first_filled_line(input_, line);
}

PreviewBlock MarkdownBuilder::started(PreviewBlockKind kind, std::uint8_t level)
{
    const bool first_of_item = marker_.has_value();
    return PreviewBlock{
        .kind = first_of_item && kind == PreviewBlockKind::paragraph ? PreviewBlockKind::list_item
                                                                     : kind,
        .level = level,
        .quote_depth = saturated_depth(quote_depth_),
        .list_depth = saturated_depth(lists_.size()),
        .marker = std::exchange(marker_, std::nullopt).value_or(std::string{}),
        .source_begin = Offset{0U},
        .text = {},
        .spans = {},
        .task = std::exchange(task_, std::nullopt),
        .table = std::nullopt,
        .alert = std::nullopt,
    };
}

PreviewBlock &MarkdownBuilder::opened(PreviewBlockKind kind, std::uint8_t level)
{
    close_implicit();
    leaf_begin_.reset();
    implicit_leaf_ = false;
    return leaf_.emplace(started(kind, level));
}

PreviewBlock &MarkdownBuilder::ensure_leaf()
{
    if (leaf_.has_value())
    {
        return leaf_.value();
    }
    auto &block = opened(PreviewBlockKind::paragraph, 0U);
    implicit_leaf_ = true;
    return block;
}

void MarkdownBuilder::close_implicit()
{
    if (implicit_leaf_)
    {
        close_leaf();
    }
}

void MarkdownBuilder::flush_marker()
{
    if (!marker_.has_value())
    {
        return;
    }
    open_leaf(PreviewBlockKind::list_item, 0U);
    close_leaf();
}
} // namespace nenenib::core
