// scope `--vim-easymotion` と `--vim-easymotion-property` の単体テスト（ADR 0163 の決定 8 の core
// の 分・#1774）。本物の Vim に無い機能なので oracle の fixture は作れず、engine
// の契約と純関数の性質で 守る。契約: `\` `\`
// の接頭辞と表に無い鍵・回数・INSERT・オペレータの後ろ・9 動作の跳び先・日本語の
// 語の切れ目・キャレットの除外・格子と表示領域の要求・1 つなら即跳ぶ・0 ならビープ・印の割り当て・
// 絞りと取消・`''`・VISUAL の伸び・`.` と undo の印を動かさない・待ちを捨てる所。性質: 固定 seed の
// 生成器（std::mt19937_64 の剰余だけ）で作った本文とキャレットで、跳び先が本文の順に重複なく
// キャレットを含まず文字の先頭にあること、印が重複なく接頭辞が重ならず数が合うこと、印の鍵の
// どの経路も「絞る / 跳ぶ / 取消」に落ち、印を打ち切れば同じ位置の跳び先へ跳ぶこと。
#include "EasyMotion.hpp"
#include "EasyMotionCancel.hpp"
#include "EasyMotionJump.hpp"
#include "EasyMotionKind.hpp"
#include "EasyMotionNarrowed.hpp"
#include "EasyMotionOutcome.hpp"
#include "EasyMotionState.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "Selection.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimEditorView.hpp"
#include "VimEffect.hpp"
#include "VimInsertString.hpp"
#include "VimKey.hpp"
#include "VimLineRows.hpp"
#include "VimMarkEdits.hpp"
#include "VimMode.hpp"
#include "VimMoveTo.hpp"
#include "VimNoEffect.hpp"
#include "VimPrefix.hpp"
#include "VimRepeatFailure.hpp"
#include "VimScrollUnit.hpp"
#include "VimSelect.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimViewport.hpp"
#include "VirtualColumn.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <random>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::EasyMotionCancel;
using core::EasyMotionJump;
using core::EasyMotionKind;
using core::EasyMotionNarrowed;
using core::EasyMotionOutcome;
using core::EasyMotionState;
using core::LineNumber;
using core::Offset;
using core::Selection;
using core::TextBuffer;
using core::VimCharacter;
using core::VimEditorView;
using core::VimEffect;
using core::VimKey;
using core::VimMode;
using core::VimMoveTo;
using core::VimPrefix;
using core::VimRepeatFailure;
using core::VimSelect;
using core::VimSpecialKey;
using core::VimState;
using core::VimStep;
using core::VimViewport;
using Random = std::mt19937_64;
using Played = std::pair<VimStep, Selection>;

constexpr char32_t backslash = 0x5C;
constexpr VimViewport whole_view{LineNumber{1}, 64, 0};

[[nodiscard]] std::vector<VimKey> keys_of(std::u32string_view typed)
{
    std::vector<VimKey> keys;
    for (const char32_t code : typed)
    {
        keys.emplace_back(VimCharacter{code});
    }
    return keys;
}

[[nodiscard]] Selection applied(const Selection &selection, const VimEffect &effect)
{
    if (const auto *const move = std::get_if<VimMoveTo>(&effect); move != nullptr)
    {
        return core::collapsed_at(move->caret);
    }
    if (const auto *const select = std::get_if<VimSelect>(&effect); select != nullptr)
    {
        return select->selection;
    }
    return selection;
}

// controller と同じに、表示領域が求められたら渡して同じ鍵を流し直す（ADR 0129 の決定 1）。
[[nodiscard]] VimStep stepped(const VimState &state, const TextBuffer &text,
                              const Selection &selection, const VimKey &key)
{
    const VimStep first = core::vim_step(state, VimEditorView{text, selection, std::nullopt}, key);
    if (!first.viewport_request)
    {
        return first;
    }
    return core::vim_step(state, VimEditorView{text, selection, whole_view}, key);
}

// 鍵の列を流した最後の step と、効果を写した後の選択。
[[nodiscard]] Played played(const TextBuffer &text, VimState state, Selection selection,
                            std::span<const VimKey> keys)
{
    VimStep step{state, core::VimNoEffect{}};
    for (const VimKey &key : keys)
    {
        step = stepped(state, text, selection, key);
        selection = applied(selection, step.effect);
        state = step.next;
    }
    return Played{std::move(step), selection};
}

[[nodiscard]] Played played_at(std::string_view body, std::size_t caret, std::u32string_view typed)
{
    const std::vector<VimKey> keys = keys_of(typed);
    return played(buffer_of(body), empty_vim_state(), core::collapsed_at(Offset{caret}), keys);
}

[[nodiscard]] std::vector<Offset> targets_of(std::string_view body, std::size_t caret,
                                             EasyMotionKind kind,
                                             std::optional<char32_t> character = std::nullopt)
{
    const TextBuffer text = buffer_of(body);
    const Selection selection = core::collapsed_at(Offset{caret});
    return core::easymotion_targets(VimEditorView{text, selection, whole_view}, kind, character);
}

[[nodiscard]] std::vector<Offset> offsets(std::initializer_list<std::size_t> values)
{
    std::vector<Offset> result;
    for (const std::size_t value : values)
    {
        result.push_back(Offset{value});
    }
    return result;
}

[[nodiscard]] bool waits_for_kind(const VimState &state, EasyMotionKind kind) noexcept
{
    return state.input_wait.has_value() &&
           std::holds_alternative<EasyMotionKind>(state.input_wait.value()) &&
           std::get<EasyMotionKind>(state.input_wait.value()) == kind;
}

[[nodiscard]] bool rests(const VimStep &step) noexcept
{
    return !step.next.input_wait.has_value() && !step.next.easy_motion.has_value();
}

[[nodiscard]] bool moved_to(const VimStep &step, std::size_t caret)
{
    const auto *const move = std::get_if<VimMoveTo>(&step.effect);
    return move != nullptr && move->caret == Offset{caret};
}

// 鍵の道（決定 1）。
void verify_prefix_keys()
{
    const TextBuffer text = buffer_of("ab cd ef");
    const Selection at_start = core::collapsed_at(Offset{0});
    const VimStep first =
        core::vim_step(empty_vim_state(), VimEditorView{text, at_start, std::nullopt},
                       VimKey{VimCharacter{backslash}});
    expect(waits_for_prefix(first.next, VimPrefix::leader) && !first.failure.has_value(),
           "the first backslash waits for the second");
    const VimStep second = core::vim_step(first.next, VimEditorView{text, at_start, std::nullopt},
                                          VimKey{VimCharacter{backslash}});
    expect(waits_for_prefix(second.next, VimPrefix::leader_leader),
           "the second backslash waits for the motion");
    const VimStep asked = core::vim_step(second.next, VimEditorView{text, at_start, std::nullopt},
                                         VimKey{VimCharacter{U'w'}});
    expect(asked.viewport_request && waits_for_prefix(asked.next, VimPrefix::leader_leader),
           "the motion asks for the viewport and keeps the state");
    const auto [labeled, kept] = played_at("ab cd ef", 0, U"\\\\w");
    expect(waits_for_kind(labeled.next, EasyMotionKind::word_start_after) &&
               labeled.next.easy_motion.has_value() &&
               labeled.next.easy_motion.value().targets == offsets({3, 6}) &&
               labeled.next.easy_motion.value().labels == std::vector<std::string>{"a", "s"} &&
               labeled.next.easy_motion.value().typed.empty() && kept == at_start,
           "\\\\w shows labels on the word starts after the caret");
    const VimStep counted = played_at("ab cd ef", 0, U"3\\\\").first;
    expect(!counted.next.count.has_value() &&
               waits_for_prefix(counted.next, VimPrefix::leader_leader),
           "the backslashes drop the count");

    const VimStep inserted = played_at("ab", 0, U"i\\").first;
    expect(inserted.next.mode == VimMode::insert && !inserted.next.input_wait.has_value() &&
               std::holds_alternative<core::VimInsertString>(inserted.effect),
           "a backslash in INSERT is a character");
    const VimStep operated = played_at("ab cd", 0, U"d\\").first;
    expect(operated.failure == VimRepeatFailure::refused && !operated.next.pending.has_value() &&
               rests(operated),
           "a backslash after an operator beeps and drops the operator");
}

[[nodiscard]] bool removes(const VimStep &step, std::size_t begin, std::size_t end)
{
    const auto *const removal = std::get_if<core::VimRemoveRange>(&step.effect);
    return removal != nullptr && removal->range.begin == Offset{begin} &&
           removal->range.end == Offset{end};
}

// `\` `\\` の待ちに当たらなかった鍵は、鳴らして待ちを捨て、普通の鍵として流し直す（ADR 0163 の
// 追補 2026-11-17 の 3）。
void verify_rerouted()
{
    const VimStep after_one = played_at("ab", 0, U"\\x").first;
    expect(after_one.failure == VimRepeatFailure::refused && rests(after_one) &&
               removes(after_one, 0, 1),
           "\\x beeps and x still removes a character");
    const VimStep after_two = played_at("ab", 0, U"\\\\x").first;
    expect(after_two.failure == VimRepeatFailure::refused && rests(after_two) &&
               removes(after_two, 0, 1),
           "\\\\x beeps and x still removes a character");
    const VimStep counted = played_at("abcd", 0, U"3\\x").first;
    expect(removes(counted, 0, 1) && !counted.next.count.has_value(),
           "the count before the backslash is dropped");
    const VimStep named = played_at("ab", 0, U"\"a\\\\x").first;
    expect(removes(named, 0, 1) && !named.next.selected_register.has_value() &&
               named.next.unnamed_register.value().text == "a",
           "the register name before the backslashes is dropped");
    expect(dot_record_is(after_one.next, std::nullopt, "x") &&
               dot_record_is(after_two.next, std::nullopt, "x") &&
               dot_record_is(counted.next, std::nullopt, "x") &&
               dot_record_is(named.next, std::nullopt, "x"),
           "only the rerouted key is recorded for .");
    const auto [visual, visual_kept] = played_at("ab cd", 0, U"vl\\\\x");
    expect(visual.next.mode == VimMode::normal && removes(visual, 0, 2) &&
               visual.failure == VimRepeatFailure::refused &&
               dot_record_is(visual.next, std::nullopt, "x"),
           "VISUAL \\\\x beeps and x removes the selection");
    const TextBuffer text = buffer_of("ab");
    const Selection at_start = core::collapsed_at(Offset{0});
    const std::vector<VimKey> normal_escape{VimKey{VimCharacter{backslash}},
                                            VimKey{VimSpecialKey::escape}};
    const VimStep left = played(text, empty_vim_state(), at_start, normal_escape).first;
    expect(!left.failure.has_value() && rests(left) && left.next.mode == VimMode::normal,
           "Esc after a backslash is a plain Esc without a beep");
    const std::vector<VimKey> visual_escape{
        VimKey{VimCharacter{U'v'}}, VimKey{VimCharacter{backslash}}, VimKey{VimSpecialKey::escape}};
    const VimStep left_visual = played(text, empty_vim_state(), at_start, visual_escape).first;
    expect(left_visual.next.mode == VimMode::normal && !left_visual.failure.has_value(),
           "Esc after a backslash leaves VISUAL");
}

// 9 動作の跳び先と日本語の語の切れ目・キャレットの除外（決定 2・3）。
void verify_targets()
{
    constexpr std::string_view body = "ab cd\n  ef\n\ngh.ij";
    expect(
        targets_of(body, 3, EasyMotionKind::word_start_after) == offsets({8, 11, 12, 14, 15}),
        "w targets word starts after the caret in text order with the empty line and punctuation");
    expect(targets_of(body, 8, EasyMotionKind::word_start_before) == offsets({3, 0}),
           "b targets word starts before the caret");
    expect(targets_of(body, 0, EasyMotionKind::word_end_after) == offsets({1, 4, 9, 13, 14, 16}),
           "e targets word ends after the caret and skips the empty line");
    expect(targets_of(body, 3, EasyMotionKind::line_below) == offsets({8, 11, 12}),
           "j targets the first non-blank of the lines below");
    expect(targets_of(body, 12, EasyMotionKind::line_above) == offsets({11, 8, 0}),
           "k targets the first non-blank of the lines above");
    expect(targets_of(body, 3, EasyMotionKind::character_after, U'e') == offsets({8}),
           "f targets the character after the caret");
    expect(targets_of("a.b.c.d", 4, EasyMotionKind::character_before, U'.') == offsets({3, 1}),
           "F targets the character before the caret");
    expect(targets_of("a.b.c.d", 3, EasyMotionKind::character_around, U'.') == offsets({5, 1}),
           "s targets the character on both sides but not under the caret");
    expect(targets_of(body, 8, EasyMotionKind::word_start_around) ==
               offsets({3, 11, 0, 14, 15, 12}),
           "l targets every word start but the caret");
    expect(targets_of(body, 3, EasyMotionKind::character_after).empty(),
           "f without a character has no targets");
    // 漢字・ひらがな・カタカナの切れ目（`w` と同じ語の分類・各 3 バイト）。
    constexpr std::string_view japanese = "日本語のテキストです";
    expect(targets_of(japanese, 0, EasyMotionKind::word_start_after) == offsets({9, 12, 24}),
           "w stops at the boundaries of kanji, hiragana and katakana");
    expect(targets_of(japanese, 0, EasyMotionKind::word_end_after) == offsets({6, 9, 21, 27}),
           "e stops at the ends of kanji, hiragana and katakana");
    expect(targets_of("あいあう", 0, EasyMotionKind::character_after, U'あ') == offsets({6}),
           "f finds a wide character");
    // 結合文字は手前の文字に含む（ADR 0053）。
    expect(targets_of("e\xCC\x81x e", 0, EasyMotionKind::character_after, U'e') == offsets({5}),
           "f does not stop inside a combined character");
}

// 見えている行の範囲・格子・行の無い本文（決定 3・6・ADR 0188）。
void verify_visible_range()
{
    const TextBuffer text = buffer_of("a\nb\nc\nd\ne");
    const Selection at_c = core::collapsed_at(Offset{4});
    expect(core::easymotion_targets(VimEditorView{text, at_c, VimViewport{LineNumber{2}, 3, 0}},
                                    EasyMotionKind::word_start_around,
                                    std::nullopt) == offsets({6, 2}),
           "only the visible lines have targets");
    VimEditorView grid{text, at_c, whole_view};
    grid.layout.unit = core::VimScrollUnit::row;
    expect(core::easymotion_targets(grid, EasyMotionKind::word_start_around, std::nullopt).empty(),
           "the grid display has no targets");
    expect(core::easymotion_targets(VimEditorView{text, at_c, std::nullopt},
                                    EasyMotionKind::word_start_around, std::nullopt)
               .empty(),
           "no viewport means no targets");
    // eol の文書は最後の改行を除いた本文を engine が見る（最後の空の行は Vim の行に無い）。
    expect(targets_of("a\nb", 0, EasyMotionKind::line_below) == offsets({2}),
           "the text without the final line break has no empty last line");
    const TextBuffer empty = buffer_of("");
    const Selection origin = core::collapsed_at(Offset{0});
    expect(core::easymotion_targets(VimEditorView{empty, origin, whole_view},
                                    EasyMotionKind::word_start_around, std::nullopt)
               .empty(),
           "a text without lines has no targets");
    std::string many;
    for (std::size_t word = 0; word < 1000; ++word)
    {
        many += "a ";
    }
    const auto cut = targets_of(many, 0, EasyMotionKind::word_start_after);
    expect(cut.size() == core::easymotion_target_limit && cut.front() == Offset{2} &&
               cut.back() == Offset{2 * core::easymotion_target_limit},
           "targets beyond 27 x 27 are cut from the start");
}

// 印を描けない跳び先は跳び先にしない（ADR 0163 の追補 2026-11-17 の 4）。折り返した行が窓の
// 画面行を使えば窓に全部入らない論理行を除き、横に送る文書は窓の横の範囲の外の桁を除く。
void verify_unseen_targets()
{
    const TextBuffer text = buffer_of("a\nb\nc");
    const Selection origin = core::collapsed_at(Offset{0});
    const std::array<core::VimLineRows, 3> rows{
        {{LineNumber{1}, 2}, {LineNumber{2}, 1}, {LineNumber{3}, 1}}};
    VimEditorView wrapped{text, origin, VimViewport{LineNumber{1}, 3, 0}};
    wrapped.layout.enabled = true;
    wrapped.layout.lines = rows;
    expect(core::easymotion_targets(wrapped, EasyMotionKind::line_below, std::nullopt) ==
               offsets({2}),
           "a line below the rows the wrapped lines fill has no target");
    const std::array<core::VimLineRows, 1> unmeasured_rows{{{LineNumber{1}, 2}}};
    wrapped.layout.lines = unmeasured_rows;
    const VimStep asked =
        core::vim_step(empty_vim_state(), wrapped, VimKey{VimCharacter{backslash}});
    const VimStep leader = core::vim_step(asked.next, wrapped, VimKey{VimCharacter{backslash}});
    const VimStep need = core::vim_step(leader.next, wrapped, VimKey{VimCharacter{U'j'}});
    expect(need.layout_request.has_value() && waits_for_prefix(need.next, VimPrefix::leader_leader),
           "the engine asks for the rows of the lines before it makes the targets");
    const TextBuffer wide = buffer_of("ab cd ef gh");
    VimEditorView scrolled{wide, origin, VimViewport{LineNumber{1}, 3, 0, 5, 0}};
    expect(core::easymotion_targets(scrolled, EasyMotionKind::word_start_after, std::nullopt) ==
               offsets({3}),
           "a target right of the window's columns has no target");
    scrolled.viewport = VimViewport{LineNumber{1}, 3, 0, 5, 4};
    expect(core::easymotion_targets(scrolled, EasyMotionKind::word_start_after, std::nullopt) ==
               offsets({6}),
           "a target left of the scrolled columns has no target");
}

// 印の割り当て（決定 3）。
void verify_labels()
{
    expect(core::easymotion_labels(0).empty(), "no targets means no labels");
    const auto three = core::easymotion_labels(3);
    expect(three == std::vector<std::string>{"a", "s", "d"}, "few targets take single keys");
    const auto full = core::easymotion_labels(27);
    expect(full.size() == 27 && full.back() == ";", "27 targets take every single key");
    const auto over = core::easymotion_labels(28);
    expect(over.size() == 28 && over.at(25) == "j" && over.at(26) == ";a" && over.at(27) == ";s",
           "28 targets turn the last key into a group");
    const auto most = core::easymotion_labels(1000);
    expect(most.size() == core::easymotion_target_limit && most.front() == "aa" &&
               most.back() == ";;",
           "labels stop at 27 x 27 pairs");
}

// 絞り・跳び・取消と `''`（決定 5）。
void verify_jumps()
{
    const auto [jumped, landed] = played_at("ab cd ef", 0, U"\\\\ws");
    expect(moved_to(jumped, 6) && rests(jumped) && landed == core::collapsed_at(Offset{6}),
           "a label key jumps to its target");
    const TextBuffer text = buffer_of("ab cd ef");
    expect(jumped.marks.has_value() &&
               jumped.marks.value().previous_context == core::vim_mark_position_of(text, Offset{0}),
           "the jump puts the previous position into ''");
    const auto [once, alone] = played_at("ab cd", 0, U"\\\\w");
    expect(moved_to(once, 3) && rests(once) && once.marks.has_value(),
           "a single target jumps without labels");
    const auto [none, stayed] = played_at("ab", 1, U"\\\\w");
    expect(none.failure == VimRepeatFailure::not_found && rests(none) &&
               std::holds_alternative<core::VimNoEffect>(none.effect),
           "no targets beep");
    const auto [missed, kept] = played_at("ab cd ef", 0, U"\\\\wz");
    expect(missed.failure == VimRepeatFailure::refused && rests(missed) &&
               kept == core::collapsed_at(Offset{0}),
           "a key on no label cancels with a beep");
    std::string body;
    for (std::size_t word = 0; word < 30; ++word)
    {
        body += "a ";
    }
    const VimStep narrowed = played_at(body, 0, U"\\\\w;").first;
    expect(narrowed.next.easy_motion.has_value() &&
               narrowed.next.easy_motion.value().typed == ";" &&
               waits_for_kind(narrowed.next, EasyMotionKind::word_start_after),
           "the first key of a pair narrows the labels");
    const VimStep paired = played_at(body, 0, U"\\\\w;s").first;
    expect(moved_to(paired, 2 * 28), "the second key of a pair jumps");
    const std::vector<VimKey> escaped{VimKey{VimCharacter{backslash}},
                                      VimKey{VimCharacter{backslash}}, VimKey{VimCharacter{U'w'}},
                                      VimKey{VimSpecialKey::escape}};
    const VimStep left =
        played(text, empty_vim_state(), core::collapsed_at(Offset{0}), escaped).first;
    expect(!left.failure.has_value() && rests(left), "Esc cancels the labels without a beep");
    const std::vector<VimKey> deleted{VimKey{VimCharacter{backslash}},
                                      VimKey{VimCharacter{backslash}}, VimKey{VimCharacter{U'w'}},
                                      VimKey{VimSpecialKey::delete_key}};
    const VimStep dropped =
        played(text, empty_vim_state(), core::collapsed_at(Offset{0}), deleted).first;
    expect(dropped.failure == VimRepeatFailure::refused && rests(dropped),
           "Del cancels the labels with a beep");
}

// f F s の文字の待ち（決定 4）。
void verify_character_wait()
{
    const VimStep waiting = played_at("abcabc", 0, U"\\\\f").first;
    expect(waits_for_kind(waiting.next, EasyMotionKind::character_after) &&
               !waiting.next.easy_motion.has_value(),
           "\\\\f waits for a character without labels");
    const VimStep labeled = played_at("abcabc ac", 0, U"\\\\sc").first;
    expect(labeled.next.easy_motion.has_value() &&
               labeled.next.easy_motion.value().targets == offsets({2, 5, 8}),
           "the character makes the targets");
    const VimStep jumped = played_at("abcabc", 3, U"\\\\Fb").first;
    expect(moved_to(jumped, 1), "\\\\F jumps back to a single character");
    const TextBuffer text = buffer_of("abc");
    const std::vector<VimKey> escaped{VimKey{VimCharacter{backslash}},
                                      VimKey{VimCharacter{backslash}}, VimKey{VimCharacter{U'f'}},
                                      VimKey{VimSpecialKey::escape}};
    const VimStep left =
        played(text, empty_vim_state(), core::collapsed_at(Offset{0}), escaped).first;
    expect(!left.failure.has_value() && rests(left), "Esc cancels the character wait");
}

// VISUAL は選択を伸ばし、`.`・undo の印・レジスタを動かさない（決定 5）。
void verify_visual_and_repeat()
{
    const auto [extended, selected] = played_at("ab cd ef", 0, U"v\\\\ws");
    expect(std::holds_alternative<VimSelect>(extended.effect) &&
               selected == Selection{Offset{0}, Offset{6}} &&
               extended.next.mode == VimMode::visual && rests(extended),
           "VISUAL extends the selection to the target");
    const auto [lines, line_selected] = played_at("ab\ncd\nef", 0, U"V\\\\js");
    expect(line_selected == Selection{Offset{0}, Offset{6}} &&
               lines.next.mode == VimMode::visual_line,
           "VISUAL by lines extends to the target line");
    const VimStep repeated = played_at("xab cd ef", 0, U"x\\\\ws").first;
    expect(moved_to(repeated, 7) && dot_record_is(repeated.next, std::nullopt, "x") &&
               !repeated.change.has_value() && !repeated.next.recording.has_value() &&
               !repeated.clipboard.has_value(),
           "the jump keeps the last change and makes no change mark");
}

// 矩形の VISUAL・レジスタの名指し・オペレータの後ろ（V-1777 の L4）。
void verify_other_entries()
{
    const TextBuffer text = buffer_of("ab cd ef");
    const std::vector<VimKey> block{
        VimKey{VimSpecialKey::control_v}, VimKey{VimCharacter{backslash}},
        VimKey{VimCharacter{backslash}}, VimKey{VimCharacter{U'w'}}, VimKey{VimCharacter{U's'}}};
    const auto [blocked, block_selected] =
        played(text, empty_vim_state(), core::collapsed_at(Offset{0}), block);
    expect(blocked.next.mode == VimMode::visual_block &&
               block_selected == Selection{Offset{0}, Offset{6}} && rests(blocked),
           "VISUAL by blocks extends to the target");
    const VimStep named = played_at("ab cd ef", 0, U"\"a\\\\ws").first;
    expect(moved_to(named, 6) && !named.next.selected_register.has_value() && rests(named),
           "a register name before the jump is dropped");
    const auto [operated, operated_at] = played_at("ab cd ef", 0, U"d\\\\w");
    expect(operated.next.mode == VimMode::normal && !operated.next.pending.has_value() &&
               rests(operated) && operated_at == core::collapsed_at(Offset{3}),
           "d\\\\w drops the operator and w moves as a plain key");
}

// 待ちを捨てる所（割り込み・入力行の取消・Vim のモードに入り直す）は印も捨てる（決定 4）。
void verify_discarded()
{
    const VimStep labeled = played_at("ab cd ef", 0, U"\\\\w").first;
    expect(!core::vim_interrupted(labeled.next).easy_motion.has_value() &&
               !core::vim_cancelled_input(labeled.next).easy_motion.has_value() &&
               !core::vim_resting_from(labeled.next, labeled.next.unnamed_register)
                    .easy_motion.has_value(),
           "interrupting, cancelling and resting drop the labels");
}

// ------------------------------------------ 性質（固定 seed）

constexpr std::array<std::string_view, 10> pieces{"a",  "b",  " ",  "\n", "日",
                                                  "の", "テ", "。", ",",  "e\xCC\x81"};
constexpr std::array<EasyMotionKind, 9> kinds{
    EasyMotionKind::word_start_after, EasyMotionKind::word_start_before,
    EasyMotionKind::word_end_after,   EasyMotionKind::line_below,
    EasyMotionKind::line_above,       EasyMotionKind::character_after,
    EasyMotionKind::character_before, EasyMotionKind::character_around,
    EasyMotionKind::word_start_around};
constexpr std::array<char32_t, 5> characters{U'a', U'b', U'日', U'。', U'e'};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] std::string random_body(Random &random)
{
    std::string body;
    const std::size_t count = below(random, 40);
    for (std::size_t piece = 0; piece < count; ++piece)
    {
        body += pieces.at(below(random, pieces.size()));
    }
    return body;
}

// 本文の中の文字の先頭（ADR 0053 の 1 文字・行ごと）。
[[nodiscard]] std::set<std::size_t> character_starts(const TextBuffer &text)
{
    std::set<std::size_t> starts;
    for (LineNumber line{1}; line.value <= text.line_count(); ++line.value)
    {
        const std::string content = text.line_text(line);
        const std::size_t start = text.line_start(line).value;
        starts.insert(start);
        for (Offset at{0}; at.value < content.size(); at = core::vim_character_end(content, at))
        {
            starts.insert(start + at.value);
        }
    }
    return starts;
}

// 行頭から at までの仮想桁（column_advance を足す・試験の側で別に数える）。
[[nodiscard]] std::size_t column_in(const TextBuffer &text, Offset at)
{
    const LineNumber line = text.line_containing(at);
    const std::string content = text.line_text(line);
    const std::size_t end = at.value - text.line_start(line).value;
    std::size_t column = 0;
    for (Offset walk{0}; walk.value < end; walk = core::next_code_point(content, walk))
    {
        column += core::column_advance(core::code_point_at(content, walk), column);
    }
    return column;
}

[[nodiscard]] std::size_t apart(std::size_t left, std::size_t right) noexcept
{
    return left < right ? right - left : left - right;
}

using Nearness = std::tuple<std::size_t, std::size_t, bool, std::size_t>;

[[nodiscard]] bool goes_around(EasyMotionKind kind) noexcept
{
    return kind == EasyMotionKind::character_around || kind == EasyMotionKind::word_start_around;
}

// キャレットからの近さ。s l は（行の差・仮想桁の差・キャレットより前か・位置）、一方向の動作は
// 本文の上でキャレットから離れたバイト数（w e j f は本文の順・b k F は逆の順になる）。
[[nodiscard]] Nearness nearness_of(const TextBuffer &text, Offset target, Offset caret, bool around)
{
    if (!around)
    {
        return Nearness{0, 0, target < caret, apart(target.value, caret.value)};
    }
    return Nearness{apart(text.line_containing(target).value, text.line_containing(caret).value),
                    apart(column_in(text, target), column_in(text, caret)), target < caret,
                    target.value};
}

// 跳び先は近い順に重複なく並ぶ。
[[nodiscard]] bool ordered_targets(const TextBuffer &text, const std::vector<Offset> &targets,
                                   Offset caret, EasyMotionKind kind)
{
    const bool around = goes_around(kind);
    for (std::size_t index = 1; index < targets.size(); ++index)
    {
        if (!(nearness_of(text, targets.at(index - 1), caret, around) <
              nearness_of(text, targets.at(index), caret, around)))
        {
            return false;
        }
    }
    return true;
}

// 跳び先はキャレットを含まず文字の先頭にある。
[[nodiscard]] bool on_characters(const std::vector<Offset> &targets, Offset caret,
                                 const std::set<std::size_t> &starts)
{
    for (const Offset target : targets)
    {
        if (target == caret || !starts.contains(target.value))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool prefix_free(const std::vector<std::string> &labels, std::size_t count)
{
    const std::set<std::string> unique(labels.begin(), labels.end());
    bool free = unique.size() == labels.size() && labels.size() == count;
    for (const std::string &label : labels)
    {
        for (const std::string &other : labels)
        {
            free = free && (label == other || !other.starts_with(label));
        }
    }
    return free;
}

// 印を打ち切ると同じ位置の跳び先へ跳び、途中は絞る（どの経路も 3 つのどれかに落ちる）。
[[nodiscard]] bool label_reaches(const EasyMotionState &state, std::size_t index)
{
    EasyMotionState current = state;
    const std::string &label = state.labels.at(index);
    for (std::size_t typed = 0; typed < label.size(); ++typed)
    {
        const EasyMotionOutcome outcome = core::easymotion_step(
            current, VimKey{VimCharacter{static_cast<char32_t>(label.at(typed))}});
        const bool last = typed + 1 == label.size();
        if (last)
        {
            const auto *const jump = std::get_if<EasyMotionJump>(&outcome);
            return jump != nullptr && jump->target == state.targets.at(index);
        }
        const auto *const narrowed = std::get_if<EasyMotionNarrowed>(&outcome);
        if (narrowed == nullptr)
        {
            return false;
        }
        current = narrowed->state;
    }
    return false;
}

[[nodiscard]] bool steps_agree(const EasyMotionState &state, Random &random)
{
    if (state.labels.empty())
    {
        return true;
    }
    const bool reached = label_reaches(state, below(random, state.labels.size()));
    const EasyMotionOutcome stray = core::easymotion_step(state, VimKey{VimCharacter{U'Z'}});
    const EasyMotionOutcome escaped = core::easymotion_step(state, VimKey{VimSpecialKey::escape});
    return reached && std::holds_alternative<EasyMotionCancel>(stray) &&
           std::holds_alternative<EasyMotionCancel>(escaped);
}

// 最初の印 `a` がいちばん近い跳び先（列の先頭）に付く。
[[nodiscard]] bool first_label_nearest(const std::vector<Offset> &targets,
                                       const std::vector<std::string> &labels)
{
    return labels.size() == targets.size() && (labels.empty() || labels.front() == "a");
}

void verify_property_samples(Random &random, std::array<std::size_t, 4> &broken)
{
    const TextBuffer text = buffer_of(random_body(random));
    const std::set<std::size_t> starts = character_starts(text);
    const std::vector<std::size_t> candidates(starts.begin(), starts.end());
    const Offset caret{candidates.at(below(random, candidates.size()))};
    const Selection selection = core::collapsed_at(caret);
    const EasyMotionKind kind = kinds.at(below(random, kinds.size()));
    const auto targets = core::easymotion_targets(VimEditorView{text, selection, whole_view}, kind,
                                                  characters.at(below(random, characters.size())));
    const auto labels = core::easymotion_labels(targets.size());
    broken.at(0) +=
        ordered_targets(text, targets, caret, kind) && on_characters(targets, caret, starts) ? 0
                                                                                             : 1;
    broken.at(1) += prefix_free(labels, targets.size()) ? 0 : 1;
    broken.at(3) += first_label_nearest(targets, labels) ? 0 : 1;
    broken.at(2) +=
        steps_agree(EasyMotionState{kind, targets, labels, std::string{}}, random) ? 0 : 1;
}
} // namespace

void verify_vim_easymotion_contracts()
{
    verify_prefix_keys();
    verify_rerouted();
    verify_targets();
    verify_visible_range();
    verify_unseen_targets();
    verify_labels();
    verify_jumps();
    verify_character_wait();
    verify_visual_and_repeat();
    verify_other_entries();
    verify_discarded();
}

void verify_vim_easymotion_property_scope()
{
    constexpr std::uint64_t seed = 998;
    constexpr std::size_t samples = 2000;
    Random random{seed};
    std::array<std::size_t, 4> broken{};
    for (std::size_t sample = 0; sample < samples; ++sample)
    {
        verify_property_samples(random, broken);
    }
    expect(broken.at(0) == 0, "targets are nearest first, unique, off the caret and on characters");
    expect(broken.at(1) == 0, "labels are unique, prefix free and as many as the targets");
    for (std::size_t count = 0; count <= core::easymotion_target_limit; ++count)
    {
        broken.at(1) += prefix_free(core::easymotion_labels(count), count) ? 0 : 1;
    }
    expect(broken.at(1) == 0, "every label count up to 27 x 27 is prefix free");
    expect(broken.at(2) == 0, "every label step narrows, jumps or cancels");
    expect(broken.at(3) == 0, "the first label goes to the nearest target");
}
} // namespace nenenib::tests
