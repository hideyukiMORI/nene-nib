// scope `--key-map` の core の分（#1081・ADR 0176 の決定 1・2・5・8）。鍵の記法の表の 1 対・
// 設定の `map` の行の読み書き・表を作る key_map_of・引く key_map_step と key_map_flush・
// 表を引くモード vim_map_mode_of の契約と、固定 seed のランダムな表と鍵の列の性質。
// application の入口と待ちの意図（#1094・決定 3・4・6・7・8）は後半: 打った鍵の入口が表を引き、
// 再生の口（press_vim_keys）と IME の確定は引かず、待ちは MapTimeout と待ちを保たない意図が流し、
// `:nmap` 系は設定を書き換えて保存する。
// 生成器は std::mt19937_64 の剰余だけを使い、分布の型は使わない。
#include "Appearance.hpp"
#include "CommitText.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorDelivery.hpp"
#include "EditorSettings.hpp"
#include "ExMapCommand.hpp"
#include "ExMapRequest.hpp"
#include "ExMapVerb.hpp"
#include "ExResult.hpp"
#include "InsertText.hpp"
#include "KeyMap.hpp"
#include "KeyMapEmit.hpp"
#include "KeyMapFailure.hpp"
#include "KeyMapPass.hpp"
#include "KeyMapPending.hpp"
#include "KeyMapStep.hpp"
#include "KeyMapping.hpp"
#include "MapRow.hpp"
#include "MapTimeout.hpp"
#include "MapTimer.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyName.hpp"
#include "VimKeyNames.hpp"
#include "VimKeyNotation.hpp"
#include "VimKeyPress.hpp"
#include "VimMapMode.hpp"
#include "VimMode.hpp"
#include "VimModeSet.hpp"
#include "VimOperator.hpp"
#include "VimPendingOperator.hpp"
#include "VimPrefix.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::KeyMap;
using core::KeyMapEmit;
using core::KeyMapFailure;
using core::KeyMapPass;
using core::KeyMapPending;
using core::KeyMapStep;
using core::MapRow;
using core::VimCharacter;
using core::VimKey;
using core::VimMapMode;
using core::VimModeSet;
using core::VimSpecialKey;
using Keys = std::vector<VimKey>;
using Random = std::mt19937_64;

constexpr std::size_t property_cases = 600U;
constexpr std::size_t longest_input = 24U;
constexpr VimModeSet normal_only{true, false, false, false, false};
constexpr VimModeSet insert_only{false, true, false, false, false};
constexpr std::array<VimMapMode, 4> map_modes{VimMapMode::normal, VimMapMode::insert,
                                              VimMapMode::visual, VimMapMode::operator_pending};
// 性質の試験の鍵の字母（記法と鍵の同じ位置）。
constexpr std::array<std::string_view, 4> alphabet_notation{"a", "b", "c", "<Esc>"};

[[nodiscard]] VimKey character(char32_t code)
{
    return VimKey{VimCharacter{code}};
}

[[nodiscard]] VimKey escape()
{
    return VimKey{VimSpecialKey::escape};
}

[[nodiscard]] Keys text_keys(std::u32string_view text)
{
    Keys keys;
    for (const char32_t code : text)
    {
        keys.push_back(character(code));
    }
    return keys;
}

[[nodiscard]] KeyMap map_of(const std::vector<MapRow> &rows, std::string leader = "\\")
{
    auto settings = core::default_editor_settings();
    settings.maps = rows;
    settings.mapleader = std::move(leader);
    auto map = core::key_map_of(settings);
    expect(map.has_value(), "the test table is accepted");
    return map.has_value() ? std::move(map).value() : KeyMap{};
}

[[nodiscard]] bool is_pass(const KeyMapStep &step)
{
    return std::holds_alternative<KeyMapPass>(step);
}

[[nodiscard]] bool is_pending(const KeyMapStep &step)
{
    return std::holds_alternative<KeyMapPending>(step);
}

[[nodiscard]] bool emits(const KeyMapStep &step, const Keys &keys, const Keys &waiting = {})
{
    const auto *emit = std::get_if<KeyMapEmit>(&step);
    return emit != nullptr && emit->keys == keys && emit->waiting == waiting;
}

[[nodiscard]] bool fails(std::string_view key, std::string_view value, KeyMapFailure failure)
{
    const auto row = core::parse_map_line(key, value);
    return !row.has_value() && row.error() == failure;
}

void verify_key_names()
{
    expect(core::vim_key_of_name("<Esc>") == VimKey{VimSpecialKey::escape} &&
               core::vim_key_of_name("<Space>") == character(U' ') &&
               core::vim_key_of_name("<NL>") == character(U'\n'),
           "named keys are read from the one generated table");
    expect(!core::vim_key_of_name("<leader>").has_value() &&
               !core::vim_key_of_name("<esc>").has_value() &&
               !core::vim_key_of_name("<C-s>").has_value() && !core::vim_key_of_name("j"),
           "<leader>, other spellings and unknown names are not in the table");
    for (const core::VimKeyName &row : core::vim_key_names)
    {
        const auto key = core::vim_key_of_name(row.text);
        expect(key.has_value() && core::vim_key_name_of(key.value()) == row.text,
               "every table name round trips through the pair");
    }
    expect(!core::vim_key_name_of(character(U'j')).has_value() &&
               core::vim_key_name_of(character(U' ')) == std::string_view{"<Space>"},
           "plain characters have no name and the space is <Space>");
}

void verify_map_lines()
{
    const auto row = core::parse_map_line("map.i.jj", "<Esc>");
    expect(row.has_value() && row.value().modes == insert_only && row.value().lhs == "jj" &&
               row.value().rhs == "<Esc>" && row.value().remap,
           "map.i.jj=<Esc> is an insert map row");
    const auto plain = core::parse_map_line("map.n.<leader>w", "!:w<CR>");
    expect(plain.has_value() && !plain.value().remap && plain.value().rhs == ":w<CR>" &&
               plain.value().lhs == "<leader>w",
           "a leading ! is noremap and <leader> stays in the text");
    expect(core::parse_map_line("map.n.<", "x").has_value() &&
               core::parse_map_line("map.n.a<b", "x").has_value() &&
               core::parse_map_line("map.n..", "x").has_value(),
           "a < that is not a name and a dot are plain characters");
    expect(fails("map..jj", "x", KeyMapFailure::bad_modes) &&
               fails("map.q.jj", "x", KeyMapFailure::bad_modes) &&
               fails("map.nn.jj", "x", KeyMapFailure::bad_modes),
           "empty, unknown and repeated mode letters are refused");
    expect(fails("map.n.abcd", "x", KeyMapFailure::long_lhs) &&
               fails("map.n.<leader>abc", "x", KeyMapFailure::long_lhs),
           "a left side of four keys is refused");
    expect(fails("map.n.", "x", KeyMapFailure::empty_keys) &&
               fails("map.n.x", "", KeyMapFailure::empty_keys) &&
               fails("map.n.x", "!", KeyMapFailure::empty_keys),
           "an empty left or right side is refused");
    expect(fails("map.n.<C-s>", "x", KeyMapFailure::unknown_key) &&
               fails("map.n.x", "a<Nope>", KeyMapFailure::unknown_key) &&
               fails("map.n.x", "\xFF", KeyMapFailure::unknown_key),
           "names outside the table and broken UTF-8 are refused");
    expect(fails("map.n", "x", KeyMapFailure::malformed_key) &&
               fails("lsp.n.x", "x", KeyMapFailure::malformed_key),
           "keys without the map.<modes>. shape are refused");
}

void verify_written_rows()
{
    const auto unwritable = [](std::string_view lhs, std::string_view rhs, bool remap)
    {
        const auto row = core::map_row_of(normal_only, lhs, rhs, remap);
        return !row.has_value() && row.error() == KeyMapFailure::unwritable;
    };
    expect(unwritable("a=b", "x", true) && unwritable(" x", "y", true) &&
               unwritable("x", "y ", true) && unwritable("x", "a\nb", false) &&
               unwritable("x", "!y", true),
           "rows that would not read back as written are refused");
    expect(!core::map_row_of(VimModeSet{}, "x", "y", true).has_value(), "a row needs a mode");
    const auto row = core::parse_map_line("map.xvn.j", "!gj");
    expect(row.has_value() && core::map_line_key(row.value()) == "map.nvx.j" &&
               core::map_line_value(row.value()) == "!gj",
           "the mode letters are written in n i v x o order");
    const auto again = core::parse_map_line(core::map_line_key(row.value_or(MapRow{})),
                                            core::map_line_value(row.value_or(MapRow{})));
    expect(again.has_value() && row.has_value() && again.value() == row.value(),
           "a written row reads back as the same row");
}

void verify_leader()
{
    expect(core::parse_map_leader("\\") == character(U'\\') &&
               core::parse_map_leader("<Space>") == character(U' ') &&
               core::parse_map_leader(",") == character(U','),
           "the leader is one key in the notation");
    for (const auto text : {"", "ab", "<leader>", "<Nope>", " ", "\t"})
    {
        const auto leader = core::parse_map_leader(text);
        expect(!leader.has_value() && leader.error() == KeyMapFailure::bad_leader,
               "the leader is exactly one known key");
    }
    const auto map = map_of({MapRow{normal_only, "<leader>w", "<leader>x", false}}, ",");
    expect(map.mappings.size() == 1 && map.mappings.front().lhs == text_keys(U",w") &&
               map.mappings.front().rhs == text_keys(U",x"),
           "<leader> is expanded on both sides when the table is made");
    auto settings = core::default_editor_settings();
    settings.mapleader = "ab";
    const auto refused = core::key_map_of(settings);
    expect(!refused.has_value() && refused.error() == KeyMapFailure::bad_leader,
           "a table with a bad leader is refused");
    settings.mapleader = "\\";
    settings.maps = {MapRow{normal_only, "a=b", "x", true}};
    expect(!core::key_map_of(settings).has_value(), "a table with an unwritable row is refused");
}

void verify_override()
{
    const auto map = map_of({MapRow{VimModeSet{true, false, true, false, false}, "x", "a", true},
                             MapRow{normal_only, "x", "b", true},
                             MapRow{VimModeSet{false, false, false, true, false}, "x", "c", true}});
    expect(map.mappings.size() == 2 && map.mappings.front().rhs == text_keys(U"b") &&
               map.mappings.back().rhs == text_keys(U"c"),
           "a later row takes the same mode and left side; v and x are one VISUAL");
    expect(
        emits(core::key_map_step(map, VimMapMode::normal, {}, character(U'x')), text_keys(U"b")) &&
            emits(core::key_map_step(map, VimMapMode::visual, {}, character(U'x')),
                  text_keys(U"c")),
        "each mode reads its last row");
}

void verify_map_modes()
{
    auto state =
        core::vim_resting_state(core::VimRegister{"", core::VimRegisterKind::uninitialized});
    expect(core::vim_map_mode_of(state) == VimMapMode::normal, "resting NORMAL maps as n");
    state.pending = core::VimPendingOperator{core::VimOperator::remove, std::nullopt};
    expect(core::vim_map_mode_of(state) == VimMapMode::operator_pending,
           "a pending operator maps as o");
    state.pending = std::nullopt;
    state.input_wait = core::VimPrefix::g;
    expect(!core::vim_map_mode_of(state).has_value(), "a one key argument is not mapped");
    state.input_wait = std::nullopt;
    state.mode = core::VimMode::insert;
    expect(core::vim_map_mode_of(state) == VimMapMode::insert, "INSERT maps as i");
    for (const auto mode :
         {core::VimMode::visual, core::VimMode::visual_line, core::VimMode::visual_block})
    {
        state.mode = mode;
        expect(core::vim_map_mode_of(state) == VimMapMode::visual, "every VISUAL maps as v");
    }
}

void verify_steps()
{
    expect(is_pass(core::key_map_step(KeyMap{}, VimMapMode::normal, {}, character(U'j'))),
           "an empty table passes every key");
    const auto jj = map_of({MapRow{insert_only, "jj", "<Esc>", false}});
    const Keys j = text_keys(U"j");
    expect(is_pending(core::key_map_step(jj, VimMapMode::insert, {}, character(U'j'))),
           "the first j of jj waits");
    expect(emits(core::key_map_step(jj, VimMapMode::insert, j, character(U'j')), {escape()}),
           "the second j emits the right side");
    expect(emits(core::key_map_step(jj, VimMapMode::insert, j, character(U'k')), text_keys(U"jk")),
           "a mismatch flows the waiting j and the key");
    expect(is_pass(core::key_map_step(jj, VimMapMode::normal, {}, character(U'j'))),
           "an insert map does not touch NORMAL");
    expect(core::key_map_flush(jj, VimMapMode::insert, j) == j,
           "the timeout flows the waiting keys as they are");
    const auto jk = map_of({MapRow{insert_only, "jk", "<Esc>", false}});
    expect(emits(core::key_map_step(jk, VimMapMode::insert, j, character(U'j')), j, j),
           "a mismatch that starts another left side waits again");
    const auto nested =
        map_of({MapRow{normal_only, "a", "b", true}, MapRow{normal_only, "b", "c", true},
                MapRow{normal_only, "q", "a", true}, MapRow{normal_only, "x", "b", false},
                MapRow{normal_only, "n", "nbb", true}});
    expect(emits(core::key_map_step(nested, VimMapMode::normal, {}, character(U'a')),
                 text_keys(U"c")) &&
               emits(core::key_map_step(nested, VimMapMode::normal, {}, character(U'q')),
                     text_keys(U"b")),
           "a map right side is looked up once and no deeper");
    expect(
        emits(core::key_map_step(nested, VimMapMode::normal, {}, character(U'x')), text_keys(U"b")),
        "a noremap right side is not looked up");
    expect(emits(core::key_map_step(nested, VimMapMode::normal, {}, character(U'n')),
                 text_keys(U"ncc")),
           "a right side that starts with its left side keeps that part");
    const auto shorter =
        map_of({MapRow{insert_only, "j", "a", false}, MapRow{insert_only, "jj", "b", false}});
    expect(is_pending(core::key_map_step(shorter, VimMapMode::insert, {}, character(U'j'))) &&
               core::key_map_flush(shorter, VimMapMode::insert, j) == text_keys(U"a"),
           "a key that is a whole left side and the start of a longer one waits");
    const auto op = map_of({MapRow{VimModeSet{false, false, false, false, true}, "w", "e", false}});
    expect(emits(core::key_map_step(op, VimMapMode::operator_pending, {}, character(U'w')),
                 text_keys(U"e")) &&
               is_pass(core::key_map_step(op, VimMapMode::normal, {}, character(U'w'))),
           "an o map acts only while an operator waits");
}

[[nodiscard]] std::size_t below(Random &random, std::size_t limit)
{
    return static_cast<std::size_t>(random() % limit);
}

[[nodiscard]] std::string random_notation(Random &random, std::size_t length)
{
    std::string text;
    for (std::size_t index = 0; index < length; ++index)
    {
        text += alphabet_notation.at(below(random, alphabet_notation.size()));
    }
    return text;
}

[[nodiscard]] MapRow random_row(Random &random)
{
    VimModeSet modes{below(random, 2) == 0, below(random, 2) == 0, below(random, 2) == 0,
                     below(random, 2) == 0, below(random, 2) == 0};
    if (core::mode_set_empty(modes))
    {
        modes.normal = true;
    }
    return MapRow{modes, random_notation(random, 1 + below(random, 3)),
                  random_notation(random, 1 + below(random, 3)), below(random, 2) == 0};
}

[[nodiscard]] Keys random_keys(Random &random)
{
    Keys keys;
    const auto length = below(random, longest_input + 1);
    for (std::size_t index = 0; index < length; ++index)
    {
        const auto pick = below(random, alphabet_notation.size());
        keys.push_back(pick + 1 == alphabet_notation.size()
                           ? escape()
                           : character(static_cast<char32_t>(U'a' + pick)));
    }
    return keys;
}

// 鍵を 1 つずつ入口に流したときの engine に届く列（待ちは最後に 1 秒の待ちで流す）。
[[nodiscard]] Keys streamed(const KeyMap &map, VimMapMode mode, const Keys &input, bool &bounded)
{
    Keys flowed;
    Keys pending;
    for (const auto &key : input)
    {
        auto step = core::key_map_step(map, mode, pending, key);
        if (auto *emit = std::get_if<KeyMapEmit>(&step))
        {
            flowed.insert(flowed.end(), emit->keys.begin(), emit->keys.end());
            pending = std::move(emit->waiting);
        }
        if (std::holds_alternative<KeyMapPending>(step))
        {
            pending.push_back(key);
        }
        if (std::holds_alternative<KeyMapPass>(step))
        {
            flowed.push_back(key);
        }
        bounded = bounded && pending.size() < core::key_map_lhs_limit;
    }
    const auto rest = core::key_map_flush(map, mode, pending);
    flowed.insert(flowed.end(), rest.begin(), rest.end());
    return flowed;
}

// input の at から一致する mode の行のうち左辺が最も長いもの。
[[nodiscard]] const core::KeyMapping *best_fit(const KeyMap &map, VimMapMode mode,
                                               const Keys &input, std::size_t at)
{
    const core::KeyMapping *best = nullptr;
    for (const auto &mapping : map.mappings)
    {
        const bool fits = core::mode_set_contains(mapping.modes, mode) &&
                          mapping.lhs.size() <= input.size() - at &&
                          std::equal(mapping.lhs.begin(), mapping.lhs.end(),
                                     input.begin() + static_cast<std::ptrdiff_t>(at));
        if (fits && (best == nullptr || mapping.lhs.size() > best->lhs.size()))
        {
            best = &mapping;
        }
    }
    return best;
}

// noremap だけの表の素朴な置き換え（左辺の最も長い一致を先頭から右辺に替える）。
[[nodiscard]] Keys replaced(const KeyMap &map, VimMapMode mode, const Keys &input)
{
    Keys result;
    std::size_t at = 0;
    while (at < input.size())
    {
        const auto *best = best_fit(map, mode, input, at);
        if (best == nullptr)
        {
            result.push_back(input.at(at));
            ++at;
            continue;
        }
        result.insert(result.end(), best->rhs.begin(), best->rhs.end());
        at += best->lhs.size();
    }
    return result;
}

void verify_key_map_property()
{
    Random random{0x1058'0A11'0B0E'5EEDULL};
    std::size_t agreed = 0;
    std::size_t matched = 0;
    bool bounded = true;
    for (std::size_t round = 0; round < property_cases; ++round)
    {
        const bool noremap_only = round % 2 == 0;
        std::vector<MapRow> rows;
        const auto count = 1 + below(random, 6);
        for (std::size_t index = 0; index < count; ++index)
        {
            auto row = random_row(random);
            row.remap = row.remap && !noremap_only;
            rows.push_back(std::move(row));
        }
        const auto map = map_of(rows);
        const auto mode = map_modes.at(below(random, map_modes.size()));
        const auto input = random_keys(random);
        const auto flowed = streamed(map, mode, input, bounded);
        agreed += flowed == core::key_map_flush(map, mode, input) ? 1U : 0U;
        matched += !noremap_only || flowed == replaced(map, mode, input) ? 1U : 0U;
    }
    expect(agreed == property_cases,
           "keys flowed one at a time equal the whole input replaced at once");
    expect(matched == property_cases,
           "a noremap table flows the longest left side replaced from the start");
    expect(bounded, "the waiting keys stay shorter than the longest left side");
}
// ---- application の入口（#1094）----

using nenenib::application::MapTimer;

constexpr VimModeSet visual_only_set{false, false, false, true, false};
constexpr VimModeSet pending_only{false, false, false, false, true};
constexpr VimModeSet normal_visual{true, false, true, false, false};
constexpr VimModeSet visual_set{false, false, true, false, false};
constexpr VimModeSet plain_set{true, false, true, false, true};

[[nodiscard]] SettingsReading mapped_settings(std::vector<MapRow> rows, std::string leader = "\\")
{
    auto settings = core::default_editor_settings();
    settings.maps = std::move(rows);
    settings.mapleader = std::move(leader);
    return std::optional<core::EditorSettings>{std::move(settings)};
}

[[nodiscard]] SettingsReading insert_escape()
{
    return mapped_settings({MapRow{insert_only, "jj", "<Esc>", false}});
}

// 見えている本文（意図を送らない・待ちを流さない）。
[[nodiscard]] std::string shown_body(EditorController &controller)
{
    return vim_body(controller.frame());
}

[[nodiscard]] std::optional<MapTimer> typed(EditorController &controller, char32_t code)
{
    return controller.apply(application::VimKeyPress{character(code)}).map_timer;
}

[[nodiscard]] bool message_is(EditorController &controller, std::string_view text)
{
    const auto &message = controller.frame().command_message;
    return message.has_value() && message.value().text() == text;
}

// `inoremap jj <Esc>`（決定 8）。2 打目で抜け、1 打目は本文に入らない。
void verify_insert_escape()
{
    Editing editing(insert_escape());
    open_vim_document(editing, "abc\n");
    auto &controller = editing.controller();
    vim_replay(controller, "i");
    expect(typed(controller, U'j') == MapTimer::start, "the first j starts the 1 second wait");
    expect(shown_body(controller) == "abc\n" &&
               controller.vim_state().mode == core::VimMode::insert,
           "the waiting j is not typed into the body");
    expect(typed(controller, U'j') == MapTimer::stop, "the second j ends the wait");
    expect(controller.vim_state().mode == core::VimMode::normal &&
               whole_vim_body(controller) == "abc\n",
           "jj leaves INSERT without typing either j");
    expect(editing.settings().writes() == 0, "typing a mapping does not save settings");
}

// 不一致の流しと待ちの合図（決定 4）。
void verify_mismatch_and_timeout()
{
    Editing editing(insert_escape());
    open_vim_document(editing, "abc\n");
    auto &controller = editing.controller();
    vim_replay(controller, "ijk");
    expect(shown_body(controller) == "jkabc\n", "j then k types both keys in order");
    vim_replay(controller, "j");
    const auto timed = controller.apply(application::MapTimeout{});
    expect(shown_body(controller) == "jkjabc\n" && timed.map_timer == MapTimer::stop,
           "MapTimeout types the waiting j and stops the timer");
    expect(controller.vim_state().mode == core::VimMode::insert, "the timeout stays in INSERT");
    const auto idle = controller.apply(application::MapTimeout{});
    expect(!idle.repaint && !idle.map_timer.has_value(),
           "a timeout with nothing waiting changes nothing");
}

[[nodiscard]] std::string mapped_body(const std::vector<MapRow> &rows, std::string text,
                                      std::string_view keys)
{
    Editing editing(mapped_settings(rows));
    open_vim_document(editing, std::move(text));
    vim_replay(editing.controller(), keys);
    return whole_vim_body(editing.controller());
}

// `noremap` は右辺を引き直さず、`map` の右辺は 1 段だけ（決定 3・追補 11）。
void verify_remap_depth()
{
    const std::vector<MapRow> rows{
        MapRow{normal_only, "<leader>a", "x", false}, MapRow{normal_only, "<leader>b", "x", true},
        MapRow{normal_only, "<leader>c", "<leader>b", true}, MapRow{normal_only, "x", "dd", true}};
    expect(mapped_body(rows, "abc\ndef\n", "\\a") == "bc\ndef\n",
           "a noremap right side runs x as x");
    expect(mapped_body(rows, "abc\ndef\n", "\\b") == "def\n",
           "a map right side is looked up once more");
    expect(mapped_body(rows, "abc\ndef\n", "\\c") == "bc\ndef\n",
           "the second lookup does not chain a third time");
}

// `<leader>` の展開と `:let mapleader=` で表を作り直す（決定 1・6・8）。
void verify_leader_change()
{
    Editing editing(mapped_settings({MapRow{normal_only, "<leader>d", "dd", true}}));
    open_vim_document(editing, "one\ntwo\nthree\n");
    auto &controller = editing.controller();
    vim_replay(controller, "\\d");
    expect(whole_vim_body(controller) == "two\nthree\n", "<leader>d with the default leader");
    static_cast<void>(run_ex(controller, "let mapleader=\",\""));
    const auto &written = editing.settings().written();
    expect(written.has_value() && written.value().mapleader == ",",
           ":let mapleader= saves the new leader");
    vim_replay(controller, ",d");
    expect(whole_vim_body(controller) == "three\n", "the rebuilt table uses the new leader");
}

// `nmap` と `imap` は別（決定 5）。OPERATOR-PENDING と VISUAL にも効く。
void verify_mode_tables()
{
    const std::vector<MapRow> normal_rows{MapRow{normal_only, "jk", "x", true}};
    expect(mapped_body(normal_rows, "abc\ndef\n", "ijk<Esc>") == "jkabc\ndef\n",
           "an nmap does not apply in INSERT");
    expect(mapped_body(normal_rows, "abc\ndef\n", "ijk<Esc>0jk") == "kabc\ndef\n",
           "the nmap applies in NORMAL");
    expect(mapped_body({MapRow{pending_only, "l", "$", true}}, "abc def\n", "ldl") == "a\n",
           "an omap applies after an operator and not as a motion");
    expect(mapped_body({MapRow{visual_only_set, "q", "d", true}}, "abcdef\n", "vlq") == "cdef\n",
           "an xmap applies in VISUAL");
}

// 通常モード（「通常 | Vim」の通常）には効かない（決定 5）。
void verify_ordinary_mode()
{
    Editing editing(insert_escape());
    open_vim_document(editing, "abc\n");
    auto &controller = editing.controller();
    static_cast<void>(applied(controller, application::SelectEditMode{core::EditMode::ordinary}));
    static_cast<void>(applied(controller, application::InsertText{"jj"}));
    expect(whole_vim_body(controller) == "jjabc\n", "ordinary typing is never mapped");
}

// 記録は engine に届いた鍵（決定 7）・再生の口は表を引かない（決定 3）。
void verify_records_and_replay()
{
    Editing dot(mapped_settings({MapRow{normal_only, "Q", "x", true}}));
    open_vim_document(dot, "abcdef\n");
    vim_replay(dot.controller(), "Q.");
    expect(whole_vim_body(dot.controller()) == "cdef\n" &&
               dot_record_is(dot.controller().vim_state(), std::nullopt, "x"),
           ". repeats the replaced key x");
    Editing replay(insert_escape());
    open_vim_document(replay, "abc\n");
    vim_normal(replay.controller(), "ijj");
    expect(whole_vim_body(replay.controller()) == "jjabc\n" &&
               replay.controller().vim_state().mode == core::VimMode::insert,
           "press_vim_keys does not look up the table");
}

// 待ちの間の IME の確定は積んだ鍵を先に流し、WorkCompleted は待ちを保つ（決定 4）。
void verify_flush_before_intents()
{
    Editing ime(insert_escape());
    open_vim_document(ime, "abc\n");
    vim_replay(ime.controller(), "ij");
    const auto committed = ime.controller().apply(application::CommitText{"jj"});
    expect(shown_body(ime.controller()) == "jjjabc\n" && committed.map_timer == MapTimer::stop,
           "an IME commit flushes the waiting j first and is not mapped");
    Editing work(insert_escape());
    open_vim_document(work, "abc\n");
    vim_replay(work.controller(), "ij");
    const auto worked = work.controller().apply(application::WorkCompleted{});
    expect(shown_body(work.controller()) == "abc\n" && !worked.map_timer.has_value(),
           "WorkCompleted keeps the waiting j");
    vim_replay(work.controller(), "j");
    expect(whole_vim_body(work.controller()) == "abc\n" &&
               work.controller().vim_state().mode == core::VimMode::normal,
           "the next key completes the wait after WorkCompleted");
}

// 使う人の操作の Ctrl+Tab の歩きは待ちを流し、窓の寸法は待ちを保つ（決定 4）。
void verify_walk_and_size()
{
    Editing walk(insert_escape());
    open_vim_document(walk, "abc\n");
    vim_replay(walk.controller(), "ij");
    const auto walked = walk.controller().apply(application::WalkRecentTab{core::TabStep::next});
    expect(shown_body(walk.controller()) == "jabc\n" && walked.map_timer == MapTimer::stop,
           "Ctrl+Tab flushes the waiting j");
    Editing sized(insert_escape());
    open_vim_document(sized, "abc\n");
    vim_replay(sized.controller(), "ij");
    const auto resized = sized.controller().apply(application::VisibleLines{vim_visible_lines});
    vim_replay(sized.controller(), "j");
    expect(!resized.map_timer.has_value() && whole_vim_body(sized.controller()) == "abc\n" &&
               sized.controller().vim_state().mode == core::VimMode::normal,
           "a window size change keeps the wait");
}

// `:nmap` 系と `:unmap` 系（決定 6）。
void verify_ex_commands()
{
    Editing editing;
    open_vim_document(editing, "abcdef\n");
    auto &controller = editing.controller();
    static_cast<void>(run_ex(controller, "nnoremap Q x"));
    const auto &written = editing.settings().written();
    expect(written.has_value() && written.value().maps.size() == 1 &&
               written.value().maps.front() == MapRow{normal_only, "Q", "x", false},
           ":nnoremap saves one settings row");
    vim_replay(controller, "Q");
    expect(whole_vim_body(controller) == "bcdef\n", "the saved row applies at once");
    static_cast<void>(run_ex(controller, "nunmap Q"));
    const auto &removed = editing.settings().written();
    expect(removed.has_value() && removed.value().maps.empty(), ":nunmap removes the row");
    const auto writes = editing.settings().writes();
    static_cast<void>(run_ex(controller, "nunmap Q"));
    expect(message_is(controller, "E31: No such mapping") && editing.bell().rings() > 0,
           "unmapping a missing row rings and says E31");
    static_cast<void>(run_ex(controller, "nmap <Bogus> x"));
    expect(message_is(controller, "Unknown key name in mapping") &&
               editing.settings().writes() == writes,
           "an unknown key name is refused without saving");
    static_cast<void>(run_ex(controller, "nmap"));
    expect(message_is(controller, "Not supported: listing mappings"),
           "the list is not offered yet");
}

[[nodiscard]] std::optional<core::ExMapRequest> request_of(std::string_view text)
{
    const auto result =
        core::evaluate_ex(text, core::default_editor_settings(), core::Appearance::dark);
    if (!result.has_value())
    {
        return std::nullopt;
    }
    return result.value().map;
}

[[nodiscard]] bool requests(std::string_view text, const core::ExMapRequest &expected)
{
    const auto request = request_of(text);
    return request.has_value() && request.value().verb == expected.verb &&
           request.value().modes == expected.modes && request.value().lhs == expected.lhs &&
           request.value().rhs == expected.rhs && request.value().remap == expected.remap;
}

[[nodiscard]] bool refused(std::string_view text)
{
    return !core::evaluate_ex(text, core::default_editor_settings(), core::Appearance::dark)
                .has_value();
}

void verify_ex_parsing()
{
    using core::ExMapVerb;
    expect(requests("nnoremap <leader>w :w<CR>",
                    {ExMapVerb::add, normal_only, "<leader>w", ":w<CR>", false}),
           ":nnoremap reads the left and right sides");
    expect(requests("no x  y z", {ExMapVerb::add, plain_set, "x", "y z", false}),
           ":no is noremap for NORMAL, VISUAL and OPERATOR-PENDING");
    expect(requests("ino jj <Esc>", {ExMapVerb::add, insert_only, "jj", "<Esc>", false}),
           ":ino is inoremap");
    expect(requests("imap jj", {ExMapVerb::list, insert_only, "jj", "", true}),
           "a left side alone lists");
    expect(requests("let mapleader = \"\\<Space>\"", {ExMapVerb::leader, {}, "", "<Space>", true}),
           ":let mapleader reads a Vim string");
    expect(requests("let mapleader=' '", {ExMapVerb::leader, {}, "", "<Space>", true}),
           "a single space leader is <Space>");
    expect(refused("map! x y") && refused("3nmap x y") && refused("let foo=1") &&
               refused("nunmap") && refused("let mapleader=,"),
           "! forms, ranges, other variables, a bare unmap and an unquoted value are refused");
    auto settings = core::default_editor_settings();
    settings.maps = {MapRow{normal_visual, "x", "a", true}};
    const auto edited =
        core::map_edited(settings, core::ExMapRequest{ExMapVerb::add, normal_only, "x", "b", true});
    expect(edited.has_value() &&
               edited.value().maps == std::vector<MapRow>{MapRow{visual_set, "x", "a", true},
                                                          MapRow{normal_only, "x", "b", true}},
           "adding takes the mode out of the older row");
    const auto candidates = core::ex_command_candidates();
    expect(std::ranges::find(candidates, "nnoremap") != candidates.end() &&
               std::ranges::find(candidates, "let mapleader=") != candidates.end(),
           "the map commands are Ctrl+P : candidates");
}
} // namespace

void verify_key_map_scope()
{
    verify_key_names();
    verify_map_lines();
    verify_written_rows();
    verify_leader();
    verify_override();
    verify_map_modes();
    verify_steps();
    verify_key_map_property();
    verify_insert_escape();
    verify_mismatch_and_timeout();
    verify_remap_depth();
    verify_leader_change();
    verify_mode_tables();
    verify_ordinary_mode();
    verify_records_and_replay();
    verify_flush_before_intents();
    verify_walk_and_size();
    verify_ex_commands();
    verify_ex_parsing();
}
} // namespace nenenib::tests
