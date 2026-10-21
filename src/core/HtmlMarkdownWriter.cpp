#include "HtmlMarkdownWriter.hpp"

#include "HtmlAttribute.hpp"
#include "HtmlToMarkdown.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <span>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::string_view strong_mark = "**";
constexpr std::string_view emphasis_mark = "*";
constexpr std::string_view code_mark = "`";
constexpr std::string_view fence = "```";
constexpr std::string_view rule_mark = "---";
constexpr std::string_view line_break_mark = "  \n";
constexpr std::string_view quote_mark = "> ";
// U+00A0（`&nbsp;`）。畳まない空白 1 つとして書く。
constexpr std::string_view no_break_space = "\xC2\xA0";
constexpr std::string_view language_prefix = "language-";
// 入れ子のリストの字下げ（決定 6）。
constexpr std::size_t list_indent = 2;
// 段落・見出し・リストの前後は空行 1 つ、行の区切りは改行 1 つ。
constexpr std::size_t blank_line = 2;
constexpr std::size_t new_line = 1;

[[nodiscard]] constexpr bool is_space(char character) noexcept
{
    return character == ' ' || character == '\t' || character == '\n' || character == '\r' ||
           character == '\f';
}

[[nodiscard]] std::size_t heading_level(HtmlTag tag) noexcept
{
    return std::size_t{std::to_underlying(tag)} - std::to_underlying(HtmlTag::heading_1) + 1U;
}

[[nodiscard]] std::string heading_mark(HtmlTag tag)
{
    return std::string(heading_level(tag), '#') + ' ';
}

// 空白でも U+00A0 でもない文字の並びの終わり。
[[nodiscard]] std::size_t run_end(std::string_view text, std::size_t at) noexcept
{
    while (at < text.size() && !is_space(text[at]) && !text.substr(at).starts_with(no_break_space))
    {
        ++at;
    }
    return at;
}

// `class="hl language-cpp"` → `cpp`。空白で区切った名前のうち language- で始まる最初の 1 つ。
// 無ければ空。
[[nodiscard]] std::string code_language(std::string_view attributes)
{
    const std::string classes = html_attribute(attributes, "class").value_or(std::string{});
    const std::string_view names(classes);
    std::size_t at = 0;
    while (at < names.size())
    {
        const std::size_t end = run_end(names, at);
        const std::string_view name = names.substr(at, end - at);
        if (name.starts_with(language_prefix))
        {
            return std::string(name.substr(language_prefix.size()));
        }
        at = end + 1U;
    }
    return {};
}

// 引用の中の空行（`> > ` の行末の空白を落とした `> >`）。
[[nodiscard]] std::string blank_quote(std::size_t depth)
{
    std::string marks;
    for (std::size_t index = 0; index < depth; ++index)
    {
        marks += index == 0 ? ">" : " >";
    }
    return marks;
}

// 暗黙に閉じる要素を探すのをやめる要素（HTML の li / td / tr / p の暗黙の終わり）。
constexpr std::array<HtmlTag, 2> list_containers{{HtmlTag::unordered_list, HtmlTag::ordered_list}};
constexpr std::array<HtmlTag, 2> row_containers{{HtmlTag::table_row, HtmlTag::table}};
constexpr std::array<HtmlTag, 1> table_containers{{HtmlTag::table}};
constexpr std::array<HtmlTag, 4> paragraph_containers{
    {HtmlTag::list_item, HtmlTag::quote, HtmlTag::table_cell, HtmlTag::division}};

[[nodiscard]] bool is_heading(HtmlTag tag) noexcept
{
    return std::to_underlying(tag) <= std::to_underlying(HtmlTag::heading_6);
}

// 逃がす文字の前に `\` を置く。
[[nodiscard]] std::string escaped(std::string_view text, std::string_view specials)
{
    std::string out;
    out.reserve(text.size());
    for (const char character : text)
    {
        if (specials.find(character) != std::string_view::npos)
        {
            out += '\\';
        }
        out += character;
    }
    return out;
}

// リンクと画像の行き先（決定 6・レビュー R4）。改行は落とす。空白・括弧・`<` `>` を含むなら
// CommonMark の `<…>` で包み、中の `\` `<` `>` を逃がす。ほかは `\` だけを逃がす。
[[nodiscard]] std::string link_destination(std::string_view target)
{
    std::string kept;
    for (const char character : target)
    {
        if (character != '\n' && character != '\r')
        {
            kept += character;
        }
    }
    constexpr std::string_view enclosed = " \t()<>";
    if (std::ranges::none_of(kept, [enclosed](char character)
                             { return enclosed.find(character) != std::string_view::npos; }))
    {
        return escaped(kept, "\\");
    }
    return "<" + escaped(kept, "\\<>") + ">";
}

// 中身を持たず、開いた要素の列に積まない要素。
[[nodiscard]] bool is_void(HtmlTag tag) noexcept
{
    return tag == HtmlTag::line_break || tag == HtmlTag::rule || tag == HtmlTag::image ||
           tag == HtmlTag::void_other;
}

[[nodiscard]] bool is_inline(HtmlTag tag) noexcept
{
    return tag == HtmlTag::strong || tag == HtmlTag::emphasis || tag == HtmlTag::anchor ||
           tag == HtmlTag::code;
}
} // namespace

std::size_t HtmlMarkdownWriter::size() const noexcept
{
    return out_.size();
}

bool HtmlMarkdownWriter::open(HtmlTag tag, std::string_view attributes)
{
    if (is_void(tag))
    {
        write_void(tag, attributes);
        return true;
    }
    close_implied(tag);
    // 入れ子の深さの上限（レビュー R1・R2）。閉じる探索と行頭の印の長さをこの上限で抑える。
    if (open_.size() >= html_depth_limit)
    {
        return false;
    }
    if (is_inline(tag))
    {
        open_inline(tag, attributes);
    }
    else
    {
        open_block(tag);
    }
    open_.push_back(tag);
    return true;
}

void HtmlMarkdownWriter::write_void(HtmlTag tag, std::string_view attributes)
{
    switch (tag)
    {
    case HtmlTag::line_break:
        write_line_break();
        return;
    case HtmlTag::rule:
        write_rule();
        return;
    case HtmlTag::image:
        write_image(attributes);
        return;
    case HtmlTag::void_other:
    case HtmlTag::strong:
    case HtmlTag::emphasis:
    case HtmlTag::anchor:
    case HtmlTag::code:
    case HtmlTag::heading_1:
    case HtmlTag::heading_2:
    case HtmlTag::heading_3:
    case HtmlTag::heading_4:
    case HtmlTag::heading_5:
    case HtmlTag::heading_6:
    case HtmlTag::paragraph:
    case HtmlTag::unordered_list:
    case HtmlTag::ordered_list:
    case HtmlTag::list_item:
    case HtmlTag::quote:
    case HtmlTag::preformatted:
    case HtmlTag::table:
    case HtmlTag::table_row:
    case HtmlTag::table_cell:
    case HtmlTag::division:
    case HtmlTag::other:
        return;
    }
}

// HTML の暗黙の終わり。前の li / td / tr が閉じないまま次が来たら、前を閉じる。塊の要素が来たら
// 開いたままの p を閉じる。閉じないままだと入れ子の深さが上限に届く。
void HtmlMarkdownWriter::close_implied(HtmlTag tag)
{
    switch (tag)
    {
    case HtmlTag::list_item:
        close_inner(HtmlTag::list_item, list_containers);
        return;
    case HtmlTag::table_cell:
        close_inner(HtmlTag::table_cell, row_containers);
        return;
    case HtmlTag::table_row:
        close_inner(HtmlTag::table_row, table_containers);
        return;
    case HtmlTag::heading_1:
    case HtmlTag::heading_2:
    case HtmlTag::heading_3:
    case HtmlTag::heading_4:
    case HtmlTag::heading_5:
    case HtmlTag::heading_6:
    case HtmlTag::paragraph:
    case HtmlTag::unordered_list:
    case HtmlTag::ordered_list:
    case HtmlTag::quote:
    case HtmlTag::preformatted:
    case HtmlTag::table:
    case HtmlTag::division:
        close_inner(HtmlTag::paragraph, paragraph_containers);
        return;
    case HtmlTag::line_break:
    case HtmlTag::rule:
    case HtmlTag::image:
    case HtmlTag::void_other:
    case HtmlTag::strong:
    case HtmlTag::emphasis:
    case HtmlTag::anchor:
    case HtmlTag::code:
    case HtmlTag::other:
        return;
    }
}

// 開いた要素を内側から見て、boundaries の要素より先に target があれば target まで閉じる。
void HtmlMarkdownWriter::close_inner(HtmlTag target, std::span<const HtmlTag> boundaries)
{
    const auto inner = std::find_if(
        open_.rbegin(), open_.rend(), [target, boundaries](HtmlTag opened)
        { return opened == target || std::ranges::find(boundaries, opened) != boundaries.end(); });
    if (inner != open_.rend() && *inner == target)
    {
        close(target);
    }
}

void HtmlMarkdownWriter::open_block(HtmlTag tag)
{
    switch (tag)
    {
    case HtmlTag::heading_1:
    case HtmlTag::heading_2:
    case HtmlTag::heading_3:
    case HtmlTag::heading_4:
    case HtmlTag::heading_5:
    case HtmlTag::heading_6:
        request_breaks(blank_line);
        pending_marks_ += heading_mark(tag);
        return;
    case HtmlTag::paragraph:
    case HtmlTag::table:
        request_breaks(blank_line);
        return;
    case HtmlTag::table_row:
    case HtmlTag::division:
        request_breaks(new_line);
        return;
    case HtmlTag::table_cell:
        // セルの文字は空白で区切る（表の記法は後続・決定 6）。行頭なら空白は出ない。
        pending_space_ = true;
        return;
    case HtmlTag::unordered_list:
    case HtmlTag::ordered_list:
        open_list(tag);
        return;
    case HtmlTag::list_item:
        open_item();
        return;
    case HtmlTag::quote:
        request_breaks(blank_line);
        ++quote_depth_;
        return;
    case HtmlTag::preformatted:
        if (preformatted_depth_++ == 0)
        {
            request_breaks(blank_line);
            fence_pending_ = true;
            // pre の開きタグの直後の改行 1 つは中身に数えない（HTML の規則）。
            skip_newline_ = true;
        }
        return;
    case HtmlTag::line_break:
    case HtmlTag::rule:
    case HtmlTag::image:
    case HtmlTag::void_other:
    case HtmlTag::strong:
    case HtmlTag::emphasis:
    case HtmlTag::anchor:
    case HtmlTag::code:
    case HtmlTag::other:
        return;
    }
}

void HtmlMarkdownWriter::open_inline(HtmlTag tag, std::string_view attributes)
{
    if (tag == HtmlTag::code)
    {
        open_code(attributes);
        return;
    }
    if (tag == HtmlTag::anchor)
    {
        // pre の中のリンクは記法を出さない（文字だけ）。
        auto target = preformatted_depth_ > 0 ? std::nullopt : html_attribute(attributes, "href");
        if (target.has_value())
        {
            pending_marks_ += '[';
        }
        links_.push_back(std::move(target));
        return;
    }
    if (preformatted_depth_ > 0)
    {
        return;
    }
    pending_marks_ += tag == HtmlTag::strong ? strong_mark : emphasis_mark;
}

void HtmlMarkdownWriter::open_list(HtmlTag tag)
{
    request_breaks(lists_.empty() ? blank_line : new_line);
    lists_.push_back(tag == HtmlTag::ordered_list ? std::optional<std::size_t>{1U} : std::nullopt);
}

void HtmlMarkdownWriter::open_item()
{
    // 前の項目が閉じていなければ open の close_implied が先に閉じている。
    request_breaks(new_line);
    const auto number = lists_.empty() ? std::nullopt : lists_.back();
    if (!number.has_value())
    {
        list_marker_ = "- ";
        return;
    }
    list_marker_ = std::to_string(number.value()) + ". ";
    lists_.back() = number.value() + 1U;
}

// pre の中の code は ``` の後ろに言語を書く。外の code は `…`。
void HtmlMarkdownWriter::open_code(std::string_view attributes)
{
    if (preformatted_depth_ == 0)
    {
        pending_marks_ += code_mark;
        return;
    }
    if (fence_pending_)
    {
        write_fence(code_language(attributes));
    }
}

void HtmlMarkdownWriter::write_image(std::string_view attributes)
{
    const auto source = html_attribute(attributes, "src");
    if (preformatted_depth_ > 0 || !source.has_value())
    {
        return;
    }
    const std::string alternative = html_attribute(attributes, "alt").value_or(std::string{});
    begin_content();
    out_ += "![" + escaped(alternative, "\\]") + "](" + link_destination(source.value()) + ")";
}

void HtmlMarkdownWriter::write_rule()
{
    if (preformatted_depth_ > 0)
    {
        return;
    }
    request_breaks(blank_line);
    begin_content();
    out_ += rule_mark;
    request_breaks(blank_line);
}

void HtmlMarkdownWriter::write_line_break()
{
    if (preformatted_depth_ > 0)
    {
        raw_character('\n');
        return;
    }
    // 先頭の br は何も残さない。
    if (out_.empty())
    {
        return;
    }
    // 見出しの中の br は空白 1 つ（見出しを 2 行に割らない）。
    if (std::ranges::any_of(open_, is_heading))
    {
        pending_space_ = true;
        return;
    }
    emit_breaks();
    if (line_start_)
    {
        write_prefix();
    }
    out_ += line_break_mark;
    line_start_ = true;
    pending_space_ = false;
}

void HtmlMarkdownWriter::close(HtmlTag tag)
{
    const auto found = std::find(open_.rbegin(), open_.rend(), tag);
    if (found == open_.rend())
    {
        return;
    }
    // 見つけた要素より内側の開いたままの要素も閉じる（決定 7）。
    const auto kept = static_cast<std::size_t>(std::distance(open_.begin(), found.base()) - 1);
    while (open_.size() > kept)
    {
        const HtmlTag inner = open_.back();
        open_.pop_back();
        end(inner);
    }
}

void HtmlMarkdownWriter::end(HtmlTag tag)
{
    switch (tag)
    {
    case HtmlTag::heading_1:
    case HtmlTag::heading_2:
    case HtmlTag::heading_3:
    case HtmlTag::heading_4:
    case HtmlTag::heading_5:
    case HtmlTag::heading_6:
        close_mark(heading_mark(tag), {});
        request_breaks(blank_line);
        return;
    case HtmlTag::paragraph:
    case HtmlTag::table:
        request_breaks(blank_line);
        return;
    case HtmlTag::table_row:
    case HtmlTag::division:
        request_breaks(new_line);
        return;
    case HtmlTag::unordered_list:
    case HtmlTag::ordered_list:
        end_list();
        return;
    case HtmlTag::quote:
        quote_depth_ = quote_depth_ == 0 ? 0 : quote_depth_ - 1U;
        request_breaks(blank_line);
        return;
    case HtmlTag::preformatted:
        end_preformatted();
        return;
    case HtmlTag::strong:
    case HtmlTag::emphasis:
    case HtmlTag::anchor:
    case HtmlTag::code:
        end_inline(tag);
        return;
    case HtmlTag::list_item:
    case HtmlTag::table_cell:
    case HtmlTag::line_break:
    case HtmlTag::rule:
    case HtmlTag::image:
    case HtmlTag::void_other:
    case HtmlTag::other:
        return;
    }
}

void HtmlMarkdownWriter::end_inline(HtmlTag tag)
{
    if (tag == HtmlTag::anchor)
    {
        if (links_.empty())
        {
            return;
        }
        const auto target = std::move(links_.back());
        links_.pop_back();
        if (target.has_value())
        {
            close_mark("[", "](" + link_destination(target.value()) + ")");
        }
        return;
    }
    if (preformatted_depth_ > 0)
    {
        return;
    }
    const std::string_view mark = tag == HtmlTag::strong     ? strong_mark
                                  : tag == HtmlTag::emphasis ? emphasis_mark
                                                             : code_mark;
    close_mark(mark, mark);
}

void HtmlMarkdownWriter::end_list()
{
    if (!lists_.empty())
    {
        lists_.pop_back();
    }
    list_marker_.reset();
    request_breaks(lists_.empty() ? blank_line : new_line);
}

void HtmlMarkdownWriter::end_preformatted()
{
    if (preformatted_depth_ == 0 || --preformatted_depth_ > 0)
    {
        return;
    }
    if (fence_pending_)
    {
        write_fence({});
    }
    if (!line_start_)
    {
        out_ += '\n';
    }
    write_prefix();
    out_ += fence;
    line_start_ = false;
    skip_newline_ = false;
    request_breaks(blank_line);
}

// 開いた記号が中身の無いまま保留なら捨て、書いた後なら閉じる記号を書く。内側の要素は先に閉じて
// いるので、保留の記号の末尾はいま閉じる要素のもの。
void HtmlMarkdownWriter::close_mark(std::string_view opening, std::string_view closing)
{
    if (!pending_marks_.empty() && pending_marks_.ends_with(opening))
    {
        pending_marks_.erase(pending_marks_.size() - opening.size());
        return;
    }
    out_ += closing;
}

void HtmlMarkdownWriter::request_breaks(std::size_t count)
{
    blank_quote_depth_ =
        pending_breaks_ == 0 ? quote_depth_ : std::min(blank_quote_depth_, quote_depth_);
    pending_breaks_ = std::max(pending_breaks_, count);
    pending_space_ = false;
}

// 保留の改行を書く。最初の中身の前の改行は書かない。空行は 1 つまで。
void HtmlMarkdownWriter::emit_breaks()
{
    const std::size_t breaks = std::exchange(pending_breaks_, 0);
    if (breaks == 0 || out_.empty())
    {
        return;
    }
    if (!line_start_)
    {
        out_ += '\n';
    }
    if (breaks >= blank_line)
    {
        out_ += blank_quote(blank_quote_depth_);
        out_ += '\n';
    }
    line_start_ = true;
}

void HtmlMarkdownWriter::begin_content()
{
    emit_breaks();
    if (line_start_)
    {
        write_prefix();
        line_start_ = false;
    }
    else if (pending_space_)
    {
        out_ += ' ';
    }
    pending_space_ = false;
    out_ += pending_marks_;
    pending_marks_.clear();
}

// 行頭の印: 引用の `> ` と、リストの字下げ（項目の最初の行は記号つき）。
void HtmlMarkdownWriter::write_prefix()
{
    for (std::size_t depth = 0; depth < quote_depth_; ++depth)
    {
        out_ += quote_mark;
    }
    const std::size_t depth = lists_.size();
    if (!list_marker_.has_value())
    {
        out_.append(depth * list_indent, ' ');
        return;
    }
    out_.append(depth == 0 ? 0 : (depth - 1U) * list_indent, ' ');
    out_ += list_marker_.value();
    list_marker_.reset();
}

// pre の最初の中身の前に ``` の行を書く。
void HtmlMarkdownWriter::write_fence(std::string_view language)
{
    emit_breaks();
    if (!line_start_)
    {
        out_ += '\n';
    }
    write_prefix();
    out_ += fence;
    out_ += language;
    out_ += '\n';
    line_start_ = true;
    fence_pending_ = false;
    pending_space_ = false;
}

void HtmlMarkdownWriter::text(std::string_view decoded)
{
    if (preformatted_depth_ > 0)
    {
        raw_text(decoded);
        return;
    }
    flowing_text(decoded);
}

void HtmlMarkdownWriter::flowing_text(std::string_view decoded)
{
    std::size_t at = 0;
    while (at < decoded.size())
    {
        if (decoded.substr(at).starts_with(no_break_space))
        {
            begin_content();
            out_ += ' ';
            at += no_break_space.size();
            continue;
        }
        if (is_space(decoded[at]))
        {
            pending_space_ = true;
            ++at;
            continue;
        }
        const std::size_t end = run_end(decoded, at);
        begin_content();
        out_ += decoded.substr(at, end - at);
        at = end;
    }
}

void HtmlMarkdownWriter::raw_text(std::string_view decoded)
{
    std::size_t at = 0;
    while (at < decoded.size())
    {
        if (decoded.substr(at).starts_with(no_break_space))
        {
            raw_character(' ');
            at += no_break_space.size();
            continue;
        }
        raw_character(decoded[at]);
        ++at;
    }
}

void HtmlMarkdownWriter::begin_raw_line()
{
    if (fence_pending_)
    {
        write_fence({});
    }
    if (line_start_)
    {
        write_prefix();
        line_start_ = false;
    }
}

// pre の中の 1 バイト。空白を畳まず、改行は行頭の印（引用・リストの字下げ）を付け直す。
void HtmlMarkdownWriter::raw_character(char character)
{
    if (character == '\r')
    {
        return;
    }
    if (character != '\n')
    {
        skip_newline_ = false;
        begin_raw_line();
        out_ += character;
        return;
    }
    if (std::exchange(skip_newline_, false))
    {
        return;
    }
    if (fence_pending_)
    {
        write_fence({});
    }
    out_ += '\n';
    line_start_ = true;
}

std::string HtmlMarkdownWriter::finished() &&
{
    // 閉じない要素は終わりで閉じたとみなす（決定 7）。
    while (!open_.empty())
    {
        const HtmlTag inner = open_.back();
        open_.pop_back();
        end(inner);
    }
    while (!out_.empty() && (out_.back() == ' ' || out_.back() == '\n'))
    {
        out_.pop_back();
    }
    return std::move(out_);
}
} // namespace nenenib::core
