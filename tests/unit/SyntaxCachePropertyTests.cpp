// scope `--syntax-cache-property` の単体テスト（#923・ADR 0042 決定 2・ADR 0156 の決定 8・9・14 と
// 追補 #827）。固定 seed の決定的な生成器で、行をまたぐ構文（ブロックコメント・raw string・3 連
// 引用符・フェンス・ヒア文字列）を含む 5 言語の文書を作り、編集（挿入・削除・行の結合と分割・
// またぐ構文の開始と終了の記号の足し消し・undo / redo）と見る位置の移動（近く・4,096 行より遠く・
// 先頭・末尾・キャレットを置いて追わせる）を混ぜた列を controller（替え玉の port）に流して、各段の
// frame で次を見る。
//   1. 見えている行の色（LineView.colors）が、素朴な基準と全行で一致する。基準は 2 つで、
//      (a) 見えている最初の行 L の syntax_backfill_lines(言語) 行前の lex_start_state から
//      lex_state で L まで進めて lex_line で塗る遡り、(b) 模型の覚え（前の frame の L とその行頭の
//      基準の状態）が続けてよいとき（L の手前 4,096 行以内・その後の編集の始まりの行が覚えの行
//      以降）にそこから進めて塗る続き。どちらかに一致すれば可で、どちらで一致したかを数える
//      （遡りの限界より前で開いた構文が続きでだけ見えるのは仕様の差で、破れではない）。
//   2. 覚えの行は L の手前 4,096 行以内か無い（字句器へ通した行の数が、遡りか続きの歩数と見えて
//      いる行の数の和に等しいことで見る）。
//   3. syntax_lexed が「見えている行の数 + 一致した基準の遡りか続きの行数」に一致する（厳密な等式・
//      checked_frame の上限の式は破れの名前を選ぶだけで判定に効かない）。
//   4. 同じ入力で同じ結果（同じ trial を 2 回流して frame の要約と数を比べる）。
//   5. 固定 seed の全 trial で、続きだけで一致した frame と遡りだけで一致した frame がどちらも 1 つ
//      以上ある（覚えを使わない退行と、模型の続きの空振りを落とす）。
// 変異の確認（手元・#923）: 捨て方を緩める（行 + 50 < 覚えの行）は命題 1 で落ち、続きを使わない
// （SyntaxLines の continues を常に偽）は命題 5 で落ちる。編集のたびに過剰に捨てるのは移動だけの
// 段で続きが残るので通る（色は正しく、性能だけの差）。
// 模型の undo の単位は EditHistory の規則（削除を伴わない挿入が直前の挿入の直後に続けば畳む・
// undo / redo で単位を閉じる）をなぞり、undo / redo の編集の始まりは単位の最初の位置。
// 生成器は std::mt19937_64 の固定 seed で、分布の型は使わず剰余で範囲に畳む（決定的）。
#include "Column.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "Language.hpp"
#include "LexState.hpp"
#include "Lexer.hpp"
#include "LineNumber.hpp"
#include "LineView.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "Scopes.hpp"
#include "ScrollLines.hpp"
#include "SelectionAnchoring.hpp"
#include "SyntaxLines.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

using Random = std::mt19937_64;
// 1 段の操作: 種類と、適用の時点の文書へ剰余で畳む 3 つの生の値（最小化で前の段を外しても範囲の
// 外を指さない）。
using Action = std::array<std::uint64_t, 4>;
// 1 つの trial: 言語の表の位置・最初の本文・操作の列。
using Trial = std::tuple<std::size_t, std::string, std::vector<Action>>;
// 破れ: 破れた命題の文と、その段の位置。
using Break = std::pair<std::string, std::size_t>;
// 模型の覚え: 行と、その行頭の基準の状態。
using ModelCache = std::optional<std::pair<std::size_t, core::LexState>>;
// 模型の undo の単位: 始まりの位置・消した長さ・入れた長さ。
using ModelEntry = std::array<std::size_t, 3>;
// 模型の履歴: 単位の列と、位置・末尾が畳める挿入か（1 / 0）。
using ModelHistory = std::pair<std::vector<ModelEntry>, std::array<std::size_t, 2>>;
// どちらの基準で一致したか: 遡りだけ・続きだけ・両方。
using Tally = std::array<std::size_t, 3>;
// 1 つの言語の素: 言語・拡張子・本文の行の型・足し消しする記号。
using LanguageKit = std::tuple<core::Language, std::string_view, std::vector<std::string_view>,
                               std::vector<std::string_view>>;
// 本文と、行（1 始まり）ごとの行頭のバイト位置（最後に本文の長さ + 1）。
using Lines = std::pair<std::string, std::vector<std::size_t>>;
// 行の範囲 [from, to)。
using LineRange = std::pair<std::size_t, std::size_t>;

// 1 つの trial を流す間の controller と模型。この試験の唯一の型（CPP-011）。
struct Session
{
    app::EditorController &controller;
    const LanguageKit &kit;
    ModelHistory history;
    ModelCache cache;
    // 前の frame の見えている最初の行。編集を近くに当てるのに使う。
    std::size_t first;
    Tally tally;
    // 前の frame の時点の本文（次の操作はこの本文に当てる）。
    Lines lines;
};

// Debug で 1 秒以内に収まる回数（#923）。50 倍の走りは trial_rounds を 50 にして手元で回し、
// commit しない。
constexpr std::uint64_t property_seed = 0x6E69'6239'3233ULL;
constexpr std::size_t trial_rounds = 1;
constexpr std::size_t shown_lines = 20;
// 4,096 行より遠くへ動ける文書に挟む埋め草の行数。
constexpr std::size_t filler_lines = 4'400;

constexpr std::size_t entry_at = 0;
constexpr std::size_t entry_removed = 1;
constexpr std::size_t entry_inserted = 2;
constexpr std::size_t history_position = 0;
constexpr std::size_t history_joinable = 1;
constexpr std::size_t tally_backfill = 0;
constexpr std::size_t tally_cache = 1;
constexpr std::size_t tally_both = 2;

// 操作の種類（Action の 0 番）。act_undo より前が編集。
// 0 は挿入（applied_edit の最後の道）。
constexpr std::uint64_t act_erase = 1;
constexpr std::uint64_t act_split = 2;
constexpr std::uint64_t act_join = 3;
constexpr std::uint64_t act_mark = 4;
constexpr std::uint64_t act_unmark = 5;
constexpr std::uint64_t act_undo = 6;
constexpr std::uint64_t act_redo = 7;
constexpr std::uint64_t act_scroll_near = 8;
constexpr std::uint64_t act_scroll_far = 9;
constexpr std::uint64_t act_top = 10;
constexpr std::uint64_t act_bottom = 11;
constexpr std::uint64_t act_jump = 12;
// 前の frame の最初の行（模型の覚えの行）より少し下へキャレットを置いて、見る位置を戻す。
constexpr std::uint64_t act_return = 13;
constexpr std::uint64_t act_kinds = 14;

// 挿入する短い断片（言語に依らない）。
constexpr std::array<std::string_view, 8> pieces{"x", "ab ", "1", "(", ")", "日本", "  ", ";"};

[[nodiscard]] std::vector<LanguageKit> language_kits()
{
    return {
        LanguageKit{core::Language::cpp,
                    "cpp",
                    {"int f(int a) {", "    return a + 1; // c", "/* block opens",
                     " * inside the block", " */", "auto s = R\"x(raw text", "still raw )x\";",
                     "const char *t = \"日本語\";", "#include <vector>", "}", ""},
                    {"/*", "*/", "R\"x(", ")x\"", "\""}},
        LanguageKit{core::Language::python,
                    "py",
                    {"def f(a):", "    return a + 1  # c", "s = \"\"\"doc opens", "inside the doc",
                     "\"\"\"", "t = '''single opens", "'''", "class C:", "    pass  # 日本語", ""},
                    {"\"\"\"", "'''", "\""}},
        LanguageKit{core::Language::markdown,
                    "md",
                    {"# Title", "text with `code` and *em*", "```cpp", "int x;", "```", "~~~",
                     "> quote 日本語", "- item", "[link](u)", ""},
                    {"```\n", "~~~\n", "`"}},
        LanguageKit{core::Language::rust,
                    "rs",
                    {"fn main() {", "    let x = 1; // c", "/* outer /* inner */ still", "*/",
                     "let s = r#\"raw opens", "\"#;", "let t = \"multi 日本語", "line\";", "}", ""},
                    {"/*", "*/", "r#\"", "\"#", "\""}},
        LanguageKit{core::Language::powershell,
                    "ps1",
                    {"function F {", "    $x = 1 # c", "<# block opens", "#>", "$h = @\"", "\"@",
                     "$s = 'single 日本語", "quote'", "}", ""},
                    {"<#", "#>", "@\"\n", "\"@", "'"}},
    };
}

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return bound == 0 ? 0 : static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] Lines lines_of(std::string text)
{
    std::vector<std::size_t> starts{0};
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (text.at(index) == '\n')
        {
            starts.push_back(index + 1);
        }
    }
    starts.push_back(text.size() + 1);
    return Lines{std::move(text), std::move(starts)};
}

[[nodiscard]] Lines current_lines(const app::EditorController &controller)
{
    return lines_of(controller.text().text());
}

[[nodiscard]] std::size_t line_count(const Lines &lines)
{
    return lines.second.size() - 1;
}

[[nodiscard]] std::string_view line_of(const Lines &lines, std::size_t line)
{
    const std::size_t begin = lines.second.at(line - 1);
    return std::string_view{lines.first}.substr(begin, lines.second.at(line) - 1 - begin);
}

[[nodiscard]] std::size_t line_start(const Lines &lines, std::size_t line)
{
    return lines.second.at(line - 1);
}

[[nodiscard]] bool continuation(char byte)
{
    return (static_cast<unsigned char>(byte) & 0xC0U) == 0x80U;
}

// 行 line の中の code point の境目を raw で選んだバイト位置（行末を含む）。
[[nodiscard]] std::size_t boundary_in(const Lines &lines, std::size_t line, std::uint64_t raw)
{
    const std::string_view body = line_of(lines, line);
    std::vector<std::size_t> cuts{0};
    for (std::size_t index = 1; index <= body.size(); ++index)
    {
        if (index == body.size() || !continuation(body.at(index)))
        {
            cuts.push_back(index);
        }
    }
    return line_start(lines, line) + cuts.at(static_cast<std::size_t>(raw % cuts.size()));
}

[[nodiscard]] std::size_t line_at(const Lines &lines, std::size_t offset)
{
    const auto found = std::ranges::upper_bound(lines.second, offset);
    return static_cast<std::size_t>(found - lines.second.begin());
}

[[nodiscard]] core::TextPosition position_of(const Lines &lines, std::size_t offset)
{
    const std::size_t line = line_at(lines, offset);
    std::size_t column = 1;
    for (std::size_t index = line_start(lines, line); index < offset; ++index)
    {
        column += continuation(lines.first.at(index)) ? 0 : 1;
    }
    return core::TextPosition{core::LineNumber{line}, core::Column{column}};
}

// 編集を当てる行: 見えている範囲の近く・前の frame の最初の行より上・どこでも、を raw で選ぶ。
[[nodiscard]] std::size_t edit_line(std::size_t first, std::size_t total, std::uint64_t raw)
{
    const std::uint64_t pick = raw % 3U;
    const auto spread = static_cast<std::size_t>((raw / 3U) % 64U);
    if (pick == 0)
    {
        return std::min(total, std::max<std::size_t>(1, first + spread / 2));
    }
    if (pick == 1)
    {
        return first > spread + 1 ? first - spread - 1 : 1;
    }
    return static_cast<std::size_t>((raw / 192U) % total) + 1;
}

[[nodiscard]] std::string filler()
{
    std::string text;
    for (std::size_t index = 0; index < filler_lines; ++index)
    {
        text += "x = 1\n";
    }
    return text;
}

// 生成する文書。行の型を並べ、長い文書は途中に埋め草を挟む（4,096 行より遠くへ動けるように）。
[[nodiscard]] std::string generated_document(Random &random, const LanguageKit &kit, bool long_text)
{
    const auto &rows = std::get<2>(kit);
    const std::size_t count = 200 + below(random, 401);
    const std::size_t filler_at = long_text ? below(random, count) : count;
    std::string text;
    for (std::size_t index = 0; index < count; ++index)
    {
        text += index == filler_at ? filler() : std::string{};
        text += rows.at(below(random, rows.size()));
        text += '\n';
    }
    return text;
}

[[nodiscard]] Action random_action(Random &random)
{
    // 移動は 4 割・編集は 5 割・undo / redo は 1 割。
    constexpr std::array<std::uint64_t, 10> moves{
        act_scroll_near, act_scroll_near, act_scroll_far, act_scroll_far, act_top,
        act_bottom,      act_jump,        act_jump,       act_return,     act_return};
    const std::size_t roll = below(random, 20);
    std::uint64_t kind = roll == 18 ? act_undo : act_redo;
    if (roll < 8)
    {
        kind = moves.at(below(random, moves.size()));
    }
    else if (roll < 18)
    {
        kind = below(random, act_undo);
    }
    return Action{kind, random(), random(), random()};
}

[[nodiscard]] Trial random_trial(Random &random, std::size_t language, bool long_text)
{
    std::string text = generated_document(random, language_kits().at(language), long_text);
    const std::size_t steps = 30 + below(random, 21);
    std::vector<Action> actions;
    actions.reserve(steps);
    for (std::size_t index = 0; index < steps; ++index)
    {
        actions.push_back(random_action(random));
    }
    return Trial{language, std::move(text), std::move(actions)};
}

// 模型の履歴へ 1 つの編集を積む（EditHistory::pushed の畳み方）。
void pushed(ModelHistory &history, const ModelEntry &entry, bool coalesce)
{
    auto &entries = history.first;
    auto &position = history.second.at(history_position);
    auto &joinable = history.second.at(history_joinable);
    entries.resize(position);
    const bool folds =
        coalesce && joinable == 1 && !entries.empty() && entries.back().at(entry_removed) == 0 &&
        entry.at(entry_removed) == 0 &&
        entries.back().at(entry_at) + entries.back().at(entry_inserted) == entry.at(entry_at);
    if (folds)
    {
        entries.back().at(entry_inserted) += entry.at(entry_inserted);
        return;
    }
    entries.push_back(entry);
    position = entries.size();
    joinable = coalesce ? 1 : 0;
}

// 模型の覚えを、編集の始まり（編集の前の本文の位置 at）の行が覚えの行より前なら捨てる（追補
// #827）。
void forget_before(Session &session, const Lines &lines, std::size_t at)
{
    if (session.cache.has_value() && line_at(lines, at) < session.cache.value().first)
    {
        session.cache.reset();
    }
}

void place(app::EditorController &controller, const Lines &lines, std::size_t offset,
           core::SelectionAnchoring anchoring)
{
    static_cast<void>(controller.apply(app::PlaceCaret{position_of(lines, offset), anchoring}));
}

// [begin, end) を inserted で置き換える（選択して打つ・空の挿入は選択を消す）。
void replaced_range(Session &session, std::size_t begin, std::size_t end, std::string_view inserted)
{
    if (begin == end && inserted.empty())
    {
        return;
    }
    const Lines &lines = session.lines;
    place(session.controller, lines, begin, core::SelectionAnchoring::collapse);
    if (end != begin)
    {
        place(session.controller, lines, end, core::SelectionAnchoring::extend);
    }
    forget_before(session, lines, begin);
    if (inserted.empty())
    {
        static_cast<void>(
            session.controller.apply(app::DeleteText{core::DeleteDirection::backward}));
        pushed(session.history, ModelEntry{begin, end - begin, 0}, false);
        return;
    }
    static_cast<void>(session.controller.apply(app::InsertText{std::string(inserted)}));
    pushed(session.history, ModelEntry{begin, end - begin, inserted.size()}, true);
}

void moved_history(Session &session, bool undo)
{
    auto &entries = session.history.first;
    auto &position = session.history.second.at(history_position);
    if (undo ? position > 0 : position < entries.size())
    {
        const auto &entry = entries.at(undo ? position - 1 : position);
        forget_before(session, session.lines, entry.at(entry_at));
        position = undo ? position - 1 : position + 1;
        session.history.second.at(history_joinable) = 0;
    }
    static_cast<void>(session.controller.apply(
        app::HistoryAction{undo ? core::HistoryDirection::undo : core::HistoryDirection::redo}));
}

// 文書の中の記号の 1 つ（raw で選んだ位置から後ろへ探して最初・無ければ前から）の位置。
[[nodiscard]] std::optional<std::size_t> marker_near(std::string_view text, std::string_view marker,
                                                     std::uint64_t raw)
{
    if (text.empty())
    {
        return std::nullopt;
    }
    const std::size_t after = text.find(marker, static_cast<std::size_t>(raw % text.size()));
    const std::size_t found = after != std::string_view::npos ? after : text.find(marker);
    return found == std::string_view::npos ? std::nullopt : std::optional{found};
}

[[nodiscard]] std::string_view marker_of(const Session &session, std::uint64_t raw)
{
    const auto &markers = std::get<3>(session.kit);
    return markers.at(static_cast<std::size_t>(raw % markers.size()));
}

// 記号を足す（行頭でだけ効く・改行を含む記号は行頭へ）か、記号の 1 つを消す。
void toggled_marker(Session &session, const Lines &lines, std::size_t line, const Action &action)
{
    const std::string_view marker = marker_of(session, action.at(3));
    const bool at_start = marker.find('\n') != std::string_view::npos;
    if (action.at(0) == act_mark)
    {
        const std::size_t where =
            at_start ? line_start(lines, line) : boundary_in(lines, line, action.at(2));
        replaced_range(session, where, where, marker);
        return;
    }
    const std::string_view plain = marker.substr(0, marker.find('\n'));
    const auto found = marker_near(lines.first, plain, action.at(2));
    if (found.has_value())
    {
        replaced_range(session, found.value(), found.value() + plain.size(), "");
    }
}

void applied_edit(Session &session, const Action &action)
{
    const Lines &lines = session.lines;
    const std::size_t total = line_count(lines);
    const std::size_t line = edit_line(session.first, total, action.at(1));
    const std::size_t at = boundary_in(lines, line, action.at(2));
    const std::uint64_t kind = action.at(0);
    if (kind == act_mark || kind == act_unmark)
    {
        toggled_marker(session, lines, line, action);
        return;
    }
    if (kind == act_erase)
    {
        // 同じ行か下の 3 行までの境目までを消す（改行を含めば行の結合になる）。
        const std::size_t other =
            std::min(total, line + static_cast<std::size_t>(action.at(3) % 4U));
        const std::size_t end = boundary_in(lines, other, action.at(3) / 4U);
        replaced_range(session, std::min(at, end), std::max(at, end), "");
        return;
    }
    if (kind == act_join)
    {
        const std::size_t end_of_line = line < total ? lines.second.at(line) - 1 : at;
        replaced_range(session, end_of_line, line < total ? end_of_line + 1 : at, "");
        return;
    }
    const std::string_view inserted =
        kind == act_split ? std::string_view{"\n"}
                          : pieces.at(static_cast<std::size_t>(action.at(3) % pieces.size()));
    replaced_range(session, at, at, inserted);
}

[[nodiscard]] std::int64_t signed_spread(std::uint64_t raw, std::int64_t base, std::uint64_t width)
{
    const auto length = base + static_cast<std::int64_t>(raw % width);
    return (raw / width) % 2U == 0 ? length : -length;
}

// 見る位置の移動。縦へ送るか、編集せずにキャレットを置いて frame に追わせる。
void moved_view(Session &session, std::uint64_t kind, const Action &action)
{
    std::int64_t lines = signed_spread(action.at(1), 1, 40);
    if (kind == act_jump || kind == act_return)
    {
        const Lines &current = session.lines;
        const std::size_t total = line_count(current);
        const std::size_t below_first =
            session.first + shown_lines + static_cast<std::size_t>(action.at(1) % 30U);
        const std::size_t line = kind == act_jump
                                     ? static_cast<std::size_t>(action.at(1) % total) + 1
                                     : std::min(total, below_first);
        place(session.controller, current, boundary_in(current, line, action.at(2)),
              core::SelectionAnchoring::collapse);
        return;
    }
    if (kind == act_scroll_far)
    {
        lines = signed_spread(action.at(1), 4'097, 400);
    }
    if (kind == act_top || kind == act_bottom)
    {
        lines = kind == act_top ? -10'000'000 : 10'000'000;
    }
    static_cast<void>(session.controller.apply(app::ScrollLines{static_cast<std::int32_t>(lines)}));
}

void applied(Session &session, const Action &action)
{
    const std::uint64_t kind = action.at(0) % act_kinds;
    if (kind < act_undo)
    {
        applied_edit(session, Action{kind, action.at(1), action.at(2), action.at(3)});
        return;
    }
    if (kind == act_undo || kind == act_redo)
    {
        moved_history(session, kind == act_undo);
        return;
    }
    moved_view(session, kind, action);
}

// 状態 state から行 [from, to) を進めた状態。
[[nodiscard]] core::LexState walked(core::Language language, core::LexState state,
                                    const Lines &lines, LineRange range)
{
    for (std::size_t line = range.first; line < range.second; ++line)
    {
        state = core::lex_state(language, state, line_of(lines, line));
    }
    return state;
}

// state を最初の行の行頭の状態として見えている行を塗った区間が、frame の区間と全行で同じか。
[[nodiscard]] bool same_colors(core::Language language, core::LexState state, const Lines &lines,
                               const app::EditorFrame &frame)
{
    for (const app::LineView &view : frame.lines)
    {
        const auto lexed = core::lex_line(language, state, line_of(lines, view.number.value));
        if (lexed.spans != view.colors)
        {
            return false;
        }
        state = lexed.out;
    }
    return true;
}

[[nodiscard]] bool shown_in_order(const Lines &lines, const app::EditorFrame &frame)
{
    const std::size_t first = frame.first_visible.value;
    if (frame.lines.empty() || first == 0 || first + frame.lines.size() - 1 > line_count(lines))
    {
        return false;
    }
    for (std::size_t index = 0; index < frame.lines.size(); ++index)
    {
        const app::LineView &view = frame.lines.at(index);
        if (view.number.value != first + index || view.text != line_of(lines, first + index))
        {
            return false;
        }
    }
    return true;
}

void digested(std::vector<std::uint64_t> &digest, const app::EditorFrame &frame)
{
    digest.push_back(frame.first_visible.value);
    digest.push_back(frame.lines.size());
    digest.push_back(frame.syntax_lexed);
    for (const app::LineView &view : frame.lines)
    {
        for (const auto &span : view.colors)
        {
            digest.push_back((span.begin.value << 20U) ^ (span.end.value << 4U) ^
                             static_cast<std::uint64_t>(span.role));
        }
    }
}

// 基準の 1 つ: 見えている最初の行の行頭の状態と、そこまで字句器へ通した行の数。
using Reference = std::pair<core::LexState, std::size_t>;

// 模型の覚えから続けた基準（続けてよくなければ nullopt）。
[[nodiscard]] std::optional<Reference> carried_reference(const Session &session, const Lines &lines,
                                                         std::size_t first)
{
    if (!session.cache.has_value())
    {
        return std::nullopt;
    }
    const auto &[line, state] = session.cache.value();
    if (line > first || first - line > app::syntax_cache_reach)
    {
        return std::nullopt;
    }
    return Reference{walked(std::get<0>(session.kit), state, lines, LineRange{line, first}),
                     first - line};
}

// 遡りの基準。
[[nodiscard]] Reference backfilled_reference(core::Language language, const Lines &lines,
                                             std::size_t first)
{
    const std::size_t back = std::min(first - 1, core::syntax_backfill_lines(language));
    return Reference{walked(language, core::lex_start_state(language, first - back == 1), lines,
                            LineRange{first - back, first}),
                     back};
}

// frame が基準と同じ数の行を字句器へ通し、同じ区間を持つか。
[[nodiscard]] bool matches(core::Language language, const std::optional<Reference> &reference,
                           const Lines &lines, const app::EditorFrame &frame)
{
    return reference.has_value() &&
           frame.syntax_lexed == frame.lines.size() + reference.value().second &&
           same_colors(language, reference.value().first, lines, frame);
}

[[nodiscard]] std::size_t tally_index(bool by_backfill, bool by_cache)
{
    if (by_backfill && by_cache)
    {
        return tally_both;
    }
    return by_cache ? tally_cache : tally_backfill;
}

// 1 段の frame を基準と比べ、模型の覚えを置き換える。破れなら命題の文を返す。
[[nodiscard]] std::optional<std::string> checked_frame(Session &session, const Lines &lines,
                                                       const app::EditorFrame &frame)
{
    if (!shown_in_order(lines, frame))
    {
        return "the frame shows the document lines in order";
    }
    const auto language = std::get<0>(session.kit);
    const std::size_t first = frame.first_visible.value;
    const Reference filled = backfilled_reference(language, lines, first);
    const auto carried = carried_reference(session, lines, first);
    const bool by_backfill = matches(language, std::optional{filled}, lines, frame);
    const bool by_cache = matches(language, carried, lines, frame);
    if (!by_backfill && !by_cache)
    {
        return frame.syntax_lexed >
                       frame.lines.size() + std::max(filled.second, app::syntax_cache_reach)
                   ? "the lexed lines stay within the visible lines and the cache reach"
                   : "the colors and the lexed lines match the backfill or the kept cache";
    }
    session.tally.at(tally_index(by_backfill, by_cache)) += 1;
    session.cache = std::pair{first, (by_cache ? carried.value_or(filled) : filled).first};
    return std::nullopt;
}

// 操作の後に frame を作らず次の操作へ続けるか（4 つに 1 つ）。見えている最初の行より上の編集の
// 後に frame を挟まず見る位置を戻す列で、捨てるべき覚えが残っていれば続きの色に出る。
[[nodiscard]] bool unframed(const Action &action)
{
    return (action.at(3) >> 62U) == 0;
}

// frame を作って基準と比べる。
[[nodiscard]] std::optional<std::string> framed(Session &session,
                                                std::vector<std::uint64_t> &digest)
{
    const auto frame = session.controller.frame();
    digested(digest, frame);
    const auto failed = checked_frame(session, session.lines, frame);
    session.first = frame.first_visible.value;
    return failed;
}

// trial を流し、最初の破れを返す。tally に一致した基準を数え、digest に各段の frame の要約を足す。
[[nodiscard]] std::optional<Break> broken_in(const Trial &trial, Tally &tally,
                                             std::vector<std::uint64_t> &digest)
{
    const auto kits = language_kits();
    Editing editing;
    Session session{editing.controller(), kits.at(std::get<0>(trial)), {}, std::nullopt, 1, {}, {}};
    session.history.second = {0, 0};
    static_cast<void>(session.controller.apply(app::VisibleLines{shown_lines}));
    const std::string path = "C:\\work\\a." + std::string(std::get<1>(session.kit));
    editing.files().hold_at(path, Bytes{std::get<1>(trial)});
    static_cast<void>(
        session.controller.apply(app::OpenDocument{core::FilePath::parse(path).value()}));
    session.lines = current_lines(session.controller);
    const auto &actions = std::get<2>(trial);
    std::optional<Break> broken;
    for (std::size_t index = 0; index <= actions.size() && !broken.has_value(); ++index)
    {
        if (index > 0)
        {
            applied(session, actions.at(index - 1));
            session.lines = current_lines(session.controller);
        }
        if (index > 0 && index < actions.size() && unframed(actions.at(index - 1)))
        {
            continue;
        }
        const auto failed = framed(session, digest);
        broken = failed.has_value() ? std::optional{Break{failed.value(), index}} : std::nullopt;
    }
    tally = session.tally;
    return broken;
}

// 操作を 1 つずつ外して、同じ命題が破れる限り外したままにする（#870 の最小化と同じ形）。
[[nodiscard]] Trial minimized(Trial trial, std::string_view invariant)
{
    for (std::size_t index = std::get<2>(trial).size(); index > 0; --index)
    {
        Trial shorter = trial;
        auto &fewer = std::get<2>(shorter);
        fewer.erase(fewer.begin() + static_cast<std::ptrdiff_t>(index - 1));
        Tally ignored{};
        std::vector<std::uint64_t> unused;
        const auto broken = broken_in(shorter, ignored, unused);
        if (broken.has_value() && broken.value().first == invariant)
        {
            trial = std::move(shorter);
        }
    }
    return trial;
}

[[nodiscard]] std::string described(std::string_view label, const Trial &trial, const Break &broken)
{
    std::string text = std::string(label) + ": " + broken.first + " at step " +
                       std::to_string(broken.second) + " / language " +
                       std::to_string(std::get<0>(trial)) + " / actions";
    for (const Action &action : std::get<2>(trial))
    {
        text += " {" + std::to_string(action.at(0) % act_kinds) + "," +
                std::to_string(action.at(1)) + "," + std::to_string(action.at(2)) + "," +
                std::to_string(action.at(3)) + "}";
    }
    return text;
}

// repeat は同じ trial をもう 1 回流して同じ結果かを見るか（命題 4・回数を抑えるため最初の trial
// だけ）。
void verify_trial(std::string_view label, const Trial &trial, Tally &total, bool repeat)
{
    Tally tally{};
    std::vector<std::uint64_t> digest;
    const auto broken = broken_in(trial, tally, digest);
    for (std::size_t index = 0; index < tally.size(); ++index)
    {
        total.at(index) += tally.at(index);
    }
    if (broken.has_value())
    {
        const Trial smallest = minimized(trial, broken.value().first);
        Tally ignored{};
        std::vector<std::uint64_t> unused;
        const auto again = broken_in(smallest, ignored, unused);
        expect(false, described(label, smallest, again.value_or(broken.value())).c_str());
        return;
    }
    expect(true, "the colors of a random edit and move sequence match a reference");
    if (!repeat)
    {
        return;
    }
    Tally repeated{};
    std::vector<std::uint64_t> second;
    static_cast<void>(broken_in(trial, repeated, second));
    expect(second == digest && repeated == tally, "the same trial gives the same frames");
}

[[nodiscard]] std::string trial_label(std::size_t round, std::size_t language, bool long_text)
{
    return "round " + std::to_string(round) + " language " + std::to_string(language) +
           (long_text ? " long" : " short");
}
} // namespace

void verify_syntax_cache_property_scope()
{
    Random random{property_seed};
    Tally total{};
    const std::size_t languages = language_kits().size();
    for (std::size_t trial = 0; trial < trial_rounds * languages; ++trial)
    {
        // 言語ごとに 1 つ。長い文書（4,096 行より遠くへ動ける）と短い文書を言語と回で交互にする。
        const std::size_t round = trial / languages;
        const std::size_t language = trial % languages;
        const bool long_text = (round + language) % 2 == 0;
        verify_trial(trial_label(round, language, long_text),
                     random_trial(random, language, long_text), total, trial == 0);
    }
    // 「両方」は覚えの距離が遡りの行数と同じ frame で、覚えを使わない製品でも数えられるので、
    // 続きだけ・遡りだけをそれぞれ 1 つ以上求める（続きを使わない変異は「両方」だけでは落ちない）。
    expect(total.at(tally_cache) > 0, "some frames continue only from the kept syntax cache");
    expect(total.at(tally_backfill) > 0, "some frames match only the backfill");
}
} // namespace nenenib::tests
