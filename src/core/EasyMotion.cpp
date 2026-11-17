#include "EasyMotion.hpp"

#include "EasyMotionCancel.hpp"
#include "EasyMotionJump.hpp"
#include "EasyMotionNarrowed.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VimCaret.hpp"
#include "VimCharacter.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimScreenGeometry.hpp"
#include "VimScrollUnit.hpp"
#include "VimViewport.hpp"
#include "VimWordClass.hpp"
#include "VimWordMotion.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::core
{
namespace
{
using Targets = std::vector<Offset>;
// 1 行の跳び先を本文の順に足す走査（動作ごとに 1 つ）。character は f F s の文字。
using LineScan = void (*)(Targets &, const TextBuffer &, LineNumber, std::optional<char32_t>);

// 近さの鍵（行の差・仮想桁の差・キャレットより前か・位置）。辞書順で小さいほど近い。
using Rank = std::tuple<std::size_t, std::size_t, bool, std::size_t>;
// キャレットの行（1 始まり）と仮想桁。
using Origin = std::pair<std::size_t, std::size_t>;

constexpr std::uint32_t blank_class = 0;
constexpr char32_t ascii_last = 0x7F;

[[nodiscard]] std::uint32_t class_at(std::string_view line, Offset at) noexcept
{
    return vim_character_class(code_point_at(line, at), VimWordClass::word);
}

// 語の頭（Vim の `w` が止まる所）。空でない種類の文字で、行頭か手前の文字と種類が違う。空行も 1 つ
// の語として止まる。
void word_starts(Targets &out, const TextBuffer &text, LineNumber line, std::optional<char32_t>)
{
    const std::string content = text.line_text(line);
    const Offset start = text.line_start(line);
    if (content.empty())
    {
        out.push_back(start);
        return;
    }
    std::uint32_t previous = blank_class;
    for (Offset at{0}; at.value < content.size(); at = vim_character_end(content, at))
    {
        const std::uint32_t current = class_at(content, at);
        if (current != blank_class && current != previous)
        {
            out.push_back(Offset{start.value + at.value});
        }
        previous = current;
    }
}

// 語の尾（Vim の `e` が止まる所）。空でない種類の文字で、行の最後の文字か次の文字と種類が違う。
void word_ends(Targets &out, const TextBuffer &text, LineNumber line, std::optional<char32_t>)
{
    const std::string content = text.line_text(line);
    const Offset start = text.line_start(line);
    for (Offset at{0}; at.value < content.size();)
    {
        const Offset next = vim_character_end(content, at);
        const std::uint32_t current = class_at(content, at);
        const bool last = next.value >= content.size() || class_at(content, next) != current;
        if (current != blank_class && last)
        {
            out.push_back(Offset{start.value + at.value});
        }
        at = next;
    }
}

// 行の最初の非空白（空白だけの行は最後の文字・空行は行頭・Vim の `^` と同じ着地）。
void first_non_blank(Targets &out, const TextBuffer &text, LineNumber line, std::optional<char32_t>)
{
    out.push_back(vim_first_non_blank(text, text.line_start(line)));
}

// character と同じ code point で始まる文字（Vim の 1 文字の先頭・結合文字は手前の文字に含む）。
void characters(Targets &out, const TextBuffer &text, LineNumber line,
                std::optional<char32_t> character)
{
    if (!character.has_value())
    {
        return;
    }
    const std::string content = text.line_text(line);
    const Offset start = text.line_start(line);
    for (Offset at{0}; at.value < content.size(); at = vim_character_end(content, at))
    {
        if (code_point_at(content, at) == character.value())
        {
            out.push_back(Offset{start.value + at.value});
        }
    }
}

[[nodiscard]] LineScan scan_of(EasyMotionKind kind) noexcept
{
    switch (kind)
    {
    case EasyMotionKind::word_start_after:
    case EasyMotionKind::word_start_before:
    case EasyMotionKind::word_start_around:
        return word_starts;
    case EasyMotionKind::word_end_after:
        return word_ends;
    case EasyMotionKind::line_below:
    case EasyMotionKind::line_above:
        return first_non_blank;
    case EasyMotionKind::character_after:
    case EasyMotionKind::character_before:
    case EasyMotionKind::character_around:
        return characters;
    }
    std::unreachable();
}

// キャレットより後ろの跳び先を取るか。
[[nodiscard]] bool reaches_after(EasyMotionKind kind) noexcept
{
    switch (kind)
    {
    case EasyMotionKind::word_start_after:
    case EasyMotionKind::word_end_after:
    case EasyMotionKind::line_below:
    case EasyMotionKind::character_after:
    case EasyMotionKind::character_around:
    case EasyMotionKind::word_start_around:
        return true;
    case EasyMotionKind::word_start_before:
    case EasyMotionKind::line_above:
    case EasyMotionKind::character_before:
        return false;
    }
    std::unreachable();
}

// キャレットより前の跳び先を取るか。
[[nodiscard]] bool reaches_before(EasyMotionKind kind) noexcept
{
    switch (kind)
    {
    case EasyMotionKind::word_start_before:
    case EasyMotionKind::line_above:
    case EasyMotionKind::character_before:
    case EasyMotionKind::character_around:
    case EasyMotionKind::word_start_around:
        return true;
    case EasyMotionKind::word_start_after:
    case EasyMotionKind::word_end_after:
    case EasyMotionKind::line_below:
    case EasyMotionKind::character_after:
        return false;
    }
    std::unreachable();
}

// j k はキャレットの行を跳び先にしない（ほかの行の最初の非空白はどれもキャレットの前か後ろ）。
[[nodiscard]] bool skips_caret_line(EasyMotionKind kind) noexcept
{
    return kind == EasyMotionKind::line_below || kind == EasyMotionKind::line_above;
}

[[nodiscard]] bool shows_targets(const VimEditorView &view) noexcept
{
    return view.layout.unit == VimScrollUnit::line && !vim_has_no_lines(view);
}

// 走査した跳び先のうち、向きに合うものだけを残す（キャレットの位置は除く・本文の順のまま）。
[[nodiscard]] Targets directed(const Targets &scanned, Offset caret, EasyMotionKind kind)
{
    const bool after = reaches_after(kind);
    const bool before = reaches_before(kind);
    Targets kept;
    kept.reserve(scanned.size());
    for (const Offset target : scanned)
    {
        if ((after && caret < target) || (before && target < caret))
        {
            kept.push_back(target);
        }
    }
    return kept;
}

// from バイト目の桁 column から to バイト目までの桁を足した桁（Tab・全角・幅 0 は
// column_advance）。
[[nodiscard]] std::size_t advanced_column(std::string_view content, std::size_t from,
                                          std::size_t column, std::size_t to) noexcept
{
    for (Offset at{from}; at.value < to && at.value < content.size();
         at = next_code_point(content, at))
    {
        column += column_advance(code_point_at(content, at), column);
    }
    return column;
}

[[nodiscard]] std::size_t difference(std::size_t left, std::size_t right) noexcept
{
    return left < right ? right - left : left - right;
}

// 本文の順の跳び先それぞれの行頭からの仮想桁（行ごとに 1 回だけ歩く）。
[[nodiscard]] std::vector<std::size_t> columns_of(const TextBuffer &text, const Targets &kept)
{
    std::vector<std::size_t> columns;
    columns.reserve(kept.size());
    LineNumber current{0};
    std::string content;
    std::size_t start = 0;
    std::size_t walked = 0;
    std::size_t column = 0;
    for (const Offset target : kept)
    {
        const LineNumber line = text.line_containing(target);
        if (!(line == current))
        {
            current = line;
            content = text.line_text(line);
            start = text.line_start(line).value;
            walked = 0;
            column = 0;
        }
        column = advanced_column(content, walked, column, target.value - start);
        walked = target.value - start;
        columns.push_back(column);
    }
    return columns;
}

// キャレットの行と仮想桁（行頭から足した桁）。
[[nodiscard]] Origin origin_of(const TextBuffer &text, Offset caret)
{
    const LineNumber line = text.line_containing(caret);
    const std::string content = text.line_text(line);
    return Origin{line.value,
                  advanced_column(content, 0, 0, caret.value - text.line_start(line).value)};
}

// 前後両方の跳び先（s l）を（行の差・仮想桁の差）の辞書順に並べる（同じならキャレットより後ろが
// 先）。kept は本文の順なので、行ごとに 1 回だけ桁を歩く。
[[nodiscard]] Targets ranked_around(const TextBuffer &text, const Targets &kept, Offset caret)
{
    const Origin origin = origin_of(text, caret);
    const std::vector<std::size_t> columns = columns_of(text, kept);
    std::vector<Rank> ranks;
    ranks.reserve(kept.size());
    for (std::size_t index = 0; index < kept.size(); ++index)
    {
        const Offset target = kept.at(index);
        ranks.emplace_back(difference(text.line_containing(target).value, origin.first),
                           difference(columns.at(index), origin.second), target < caret,
                           target.value);
    }
    std::sort(ranks.begin(), ranks.end());
    Targets nearest;
    nearest.reserve(ranks.size());
    for (const Rank &rank : ranks)
    {
        nearest.push_back(Offset{std::get<3>(rank)});
    }
    return nearest;
}

// 横に送る文書（wrap off・viewport.columns が 0 でない）では、窓の横の範囲 [left_column,
// left_column + columns) の外の桁の跳び先を除く（印を描けない跳び先は跳び先にしない・ADR 0163 の
// 追補 2026-11-17 の 4・巨大な行の窓の外もこれで外れる）。
[[nodiscard]] Targets on_screen(const TextBuffer &text, const Targets &kept,
                                const VimViewport &viewport)
{
    if (viewport.columns == 0)
    {
        return kept;
    }
    const std::vector<std::size_t> columns = columns_of(text, kept);
    Targets shown;
    shown.reserve(kept.size());
    for (std::size_t index = 0; index < kept.size(); ++index)
    {
        const std::size_t column = columns.at(index);
        if (column >= viewport.left_column && column < viewport.left_column + viewport.columns)
        {
            shown.push_back(kept.at(index));
        }
    }
    return shown;
}

// 見えている行（first_visible から窓に全部入っている最後の論理行まで）を走査する。
[[nodiscard]] Targets scanned_lines(const VimEditorView &view, EasyMotionKind kind,
                                    std::optional<char32_t> character, LineNumber last)
{
    const LineScan scan = scan_of(kind);
    const LineNumber caret_line = view.text.line_containing(view.selection.caret);
    Targets scanned;
    for (LineNumber line = view.viewport.value_or(VimViewport{LineNumber{1}, 1, 0}).first_visible;
         line.value <= last.value; ++line.value)
    {
        if (!(skips_caret_line(kind) && line == caret_line))
        {
            scan(scanned, view.text, line, character);
        }
    }
    return scanned;
}

// キャレットから近い順に並べてから多くとも easymotion_target_limit 個に切る（ADR 0163 の追補
// 2026-11-17 の 2）。後ろだけの動作（w e j f）は本文の順、前だけの動作（b k F）は本文の逆の順、
// 前後両方（s l）は ranked_around。
[[nodiscard]] Targets ranked(const TextBuffer &text, Targets kept, Offset caret,
                             EasyMotionKind kind)
{
    if (reaches_after(kind) && reaches_before(kind))
    {
        kept = ranked_around(text, kept, caret);
    }
    else if (reaches_before(kind))
    {
        // std::reverse は MSVC STL の __std_reverse_* を core
        // の外へ出す（ARC-003）ので、逆順の写しを作る。
        kept = Targets(kept.rbegin(), kept.rend());
    }
    if (kept.size() > easymotion_target_limit)
    {
        kept.resize(easymotion_target_limit);
    }
    return kept;
}

// 27 を超えたときの組の鍵の数（1 文字の印を組に替えるたびに 26 個ずつ増える・最小の数）。
[[nodiscard]] std::size_t group_count(std::size_t count) noexcept
{
    const std::size_t keys = easymotion_keys.size();
    if (count <= keys)
    {
        return 0;
    }
    return (count - keys + keys - 2) / (keys - 1);
}

[[nodiscard]] bool starts_with(std::string_view label, std::string_view typed) noexcept
{
    return label.substr(0, typed.size()) == typed;
}

// 打った印の文字（ASCII だけが印の文字になる）。印の文字でなければ空。
[[nodiscard]] std::optional<char> label_character(const VimKey &key) noexcept
{
    const auto *const character = std::get_if<VimCharacter>(&key);
    if (character == nullptr || character->code > ascii_last)
    {
        return std::nullopt;
    }
    const char ascii = static_cast<char>(character->code);
    if (easymotion_keys.find(ascii) == std::string_view::npos)
    {
        return std::nullopt;
    }
    return ascii;
}
} // namespace

std::vector<Offset> easymotion_targets(const VimEditorView &view, EasyMotionKind kind,
                                       std::optional<char32_t> character)
{
    if (!view.viewport.has_value() || !shows_targets(view))
    {
        return {};
    }
    const VimViewport viewport = view.viewport.value();
    const auto last = vim_window_last_line(view);
    if (!last)
    {
        return {};
    }
    const Targets scanned = scanned_lines(view, kind, character, last.value());
    const Targets kept =
        on_screen(view.text, directed(scanned, view.selection.caret, kind), viewport);
    return ranked(view.text, kept, view.selection.caret, kind);
}

std::vector<std::string> easymotion_labels(std::size_t count)
{
    const std::size_t wanted = std::min(count, easymotion_target_limit);
    const std::size_t groups = group_count(wanted);
    const std::size_t singles = easymotion_keys.size() - groups;
    std::vector<std::string> labels;
    labels.reserve(wanted);
    for (std::size_t key = 0; key < singles && labels.size() < wanted; ++key)
    {
        labels.emplace_back(1, easymotion_keys.at(key));
    }
    for (std::size_t group = singles; group < easymotion_keys.size(); ++group)
    {
        for (std::size_t key = 0; key < easymotion_keys.size() && labels.size() < wanted; ++key)
        {
            labels.push_back(std::string{easymotion_keys.at(group), easymotion_keys.at(key)});
        }
    }
    return labels;
}

EasyMotionOutcome easymotion_step(const EasyMotionState &state, const VimKey &key)
{
    const auto character = label_character(key);
    if (!character.has_value())
    {
        return EasyMotionCancel{};
    }
    std::string typed = state.typed + character.value();
    bool continues = false;
    for (std::size_t index = 0; index < state.labels.size(); ++index)
    {
        if (state.labels.at(index) == typed && index < state.targets.size())
        {
            return EasyMotionJump{state.targets.at(index)};
        }
        continues = continues || starts_with(state.labels.at(index), typed);
    }
    if (!continues)
    {
        return EasyMotionCancel{};
    }
    EasyMotionState narrowed = state;
    narrowed.typed = std::move(typed);
    return EasyMotionNarrowed{std::move(narrowed)};
}
} // namespace nenenib::core
