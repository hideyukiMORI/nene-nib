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
#include "EndSession.hpp"
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
#include "SessionEnd.hpp"
#include "StoreVimRegister.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "VimCharacter.hpp"
#include "VimCount.hpp"
#include "VimKey.hpp"
#include "VimKeyName.hpp"
#include "VimKeyNames.hpp"
#include "VimKeyNotation.hpp"
#include "VimKeyPress.hpp"
#include "VimMapMode.hpp"
#include "VimMode.hpp"
#include "VimModeSet.hpp"
#include "VimNamedRegisters.hpp"
#include "VimOperator.hpp"
#include "VimPendingOperator.hpp"
#include "VimPrefix.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimRegisterText.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <random>
#include <span>
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

// 1 単位の Emit で、鍵の列が keys で、引き直しの始まりが remap か（#1276）。
[[nodiscard]] bool remaps_from(const KeyMapStep &step, const Keys &keys, std::size_t remap)
{
    const auto *emit = std::get_if<KeyMapEmit>(&step);
    return emit != nullptr && emit->keys == keys && emit->remaps == std::vector<std::size_t>{remap};
}

// 右辺の残り rest を mode のまま rhs_unit_step で頭から引き直した列（#1276）。
[[nodiscard]] Keys rhs_looked_up(const KeyMap &map, std::optional<VimMapMode> mode, Keys rest)
{
    Keys flowed;
    std::span<const VimKey> left{rest};
    while (!left.empty())
    {
        const auto [count, found] = core::rhs_unit_step(map, mode, left);
        flowed.insert(flowed.end(), found.begin(), found.end());
        left = left.subspan(count);
    }
    return flowed;
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
    expect(core::vim_map_mode_of(state, character(U'j')) == VimMapMode::normal,
           "resting NORMAL maps as n");
    state.pending = core::VimPendingOperator{core::VimOperator::remove, std::nullopt};
    expect(core::vim_map_mode_of(state, character(U'j')) == VimMapMode::operator_pending,
           "a pending operator maps as o");
    state.pending = std::nullopt;
    state.input_wait = core::VimPrefix::g;
    expect(!core::vim_map_mode_of(state, character(U'j')).has_value(),
           "a one key argument is not mapped");
    state.input_wait = std::nullopt;
    state.mode = core::VimMode::insert;
    expect(core::vim_map_mode_of(state, character(U'j')) == VimMapMode::insert, "INSERT maps as i");
    for (const auto mode :
         {core::VimMode::visual, core::VimMode::visual_line, core::VimMode::visual_block})
    {
        state.mode = mode;
        expect(core::vim_map_mode_of(state, character(U'j')) == VimMapMode::visual,
               "every VISUAL maps as v");
    }
}

// 回数の続きの `0` は表を引かない（Vim の no_zero_mapping・normal.c の normal_cmd_get_count・
// getchar.c の handle_mapping・#1311）。回数の先頭の `0` とオペレータの後の `0` は引く。
void verify_zero_in_count()
{
    auto state =
        core::vim_resting_state(core::VimRegister{"", core::VimRegisterKind::uninitialized});
    const auto zero = character(U'0');
    expect(core::vim_map_mode_of(state, zero) == VimMapMode::normal,
           "a 0 without a count maps as n");
    state.count = core::VimCount{5};
    expect(!core::vim_map_mode_of(state, zero).has_value() &&
               core::vim_continues_count(state, zero),
           "a 0 after a count is not mapped");
    expect(core::vim_map_mode_of(state, character(U'1')) == VimMapMode::normal &&
               core::vim_map_mode_of(state, core::VimKey{core::VimSpecialKey::escape}) ==
                   VimMapMode::normal,
           "other keys after a count are mapped");
    state.mode = core::VimMode::visual_line;
    expect(!core::vim_map_mode_of(state, zero).has_value(), "the same holds in VISUAL");
    state.mode = core::VimMode::normal;
    state.pending = core::VimPendingOperator{core::VimOperator::remove, core::VimCount{2}};
    state.count = std::nullopt;
    expect(core::vim_map_mode_of(state, zero) == VimMapMode::operator_pending,
           "the 0 after 2d is a motion and maps as o");
    state.count = core::VimCount{1};
    expect(!core::vim_map_mode_of(state, zero).has_value(), "the 0 of d10 is not mapped");
    state.pending = std::nullopt;
    state.mode = core::VimMode::insert;
    expect(core::vim_map_mode_of(state, zero) == VimMapMode::insert &&
               !core::vim_continues_count(state, zero),
           "INSERT maps a 0 whatever the count");
}

// 1 段の引き直しの表（a → b → c・q → a・noremap の x → b・左辺で始まる n → nbb）。
[[nodiscard]] KeyMap nested_map()
{
    return map_of({MapRow{normal_only, "a", "b", true}, MapRow{normal_only, "b", "c", true},
                   MapRow{normal_only, "q", "a", true}, MapRow{normal_only, "x", "b", false},
                   MapRow{normal_only, "n", "nbb", true}});
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
    expect(core::key_map_flush(jj, VimMapMode::insert, j).keys == j,
           "the timeout flows the waiting keys as they are");
    const auto jk = map_of({MapRow{insert_only, "jk", "<Esc>", false}});
    expect(emits(core::key_map_step(jk, VimMapMode::insert, j, character(U'j')), j, j),
           "a mismatch that starts another left side waits again");
    const auto nested = nested_map();
    const auto shorter =
        map_of({MapRow{insert_only, "j", "a", false}, MapRow{insert_only, "jj", "b", false}});
    expect(is_pending(core::key_map_step(shorter, VimMapMode::insert, {}, character(U'j'))) &&
               core::key_map_flush(shorter, VimMapMode::insert, j).keys == text_keys(U"a"),
           "a key that is a whole left side and the start of a longer one waits");
    const auto matched = core::key_map_step(jj, VimMapMode::insert, j, character(U'j'));
    const auto *units = std::get_if<KeyMapEmit>(&matched);
    expect(units != nullptr && units->ends == std::vector<std::size_t>{1U} &&
               units->reads == std::vector<std::size_t>{2U},
           "a matched right side is one unit");
    const auto split = core::key_map_step(jj, VimMapMode::insert, j, character(U'k'));
    const auto *typed_units = std::get_if<KeyMapEmit>(&split);
    expect(typed_units != nullptr && typed_units->ends == std::vector<std::size_t>{1U, 2U} &&
               typed_units->reads == std::vector<std::size_t>{1U, 2U},
           "each key not in the table is its own unit");
    const auto chained = core::key_map_step(nested, VimMapMode::normal, {}, character(U'n'));
    const auto *chained_units = std::get_if<KeyMapEmit>(&chained);
    expect(chained_units != nullptr && chained_units->ends == std::vector<std::size_t>{3U} &&
               chained_units->reads == std::vector<std::size_t>{1U},
           "a right side looked up once more stays one unit");
    expect(core::key_map_flush(shorter, VimMapMode::insert, j).ends == std::vector<std::size_t>{1U},
           "the timeout flush marks the same units");
    const auto op = map_of({MapRow{VimModeSet{false, false, false, false, true}, "w", "e", false}});
    expect(emits(core::key_map_step(op, VimMapMode::operator_pending, {}, character(U'w')),
                 text_keys(U"e")) &&
               is_pass(core::key_map_step(op, VimMapMode::normal, {}, character(U'w'))),
           "an o map acts only while an operator waits");
}

// 右辺は生のまま返り、1 段の引き直しは rhs_unit_step が与えたモードで行う（#1276）。
void verify_raw_right_sides()
{
    const auto nested = nested_map();
    expect(remaps_from(core::key_map_step(nested, VimMapMode::normal, {}, character(U'a')),
                       text_keys(U"b"), 0U) &&
               remaps_from(core::key_map_step(nested, VimMapMode::normal, {}, character(U'q')),
                           text_keys(U"a"), 0U),
           "a map right side is emitted raw and looked up again from its start");
    expect(remaps_from(core::key_map_step(nested, VimMapMode::normal, {}, character(U'x')),
                       text_keys(U"b"), 1U),
           "a noremap right side is not looked up");
    expect(remaps_from(core::key_map_step(nested, VimMapMode::normal, {}, character(U'n')),
                       text_keys(U"nbb"), 1U),
           "a right side that starts with its left side keeps that part");
    expect(rhs_looked_up(nested, VimMapMode::normal, text_keys(U"a")) == text_keys(U"b") &&
               rhs_looked_up(nested, VimMapMode::normal, text_keys(U"bb")) == text_keys(U"cc"),
           "a right side is looked up once and no deeper");
    expect(rhs_looked_up(nested, VimMapMode::insert, text_keys(U"bb")) == text_keys(U"bb") &&
               rhs_looked_up(nested, std::nullopt, text_keys(U"bb")) == text_keys(U"bb"),
           "the lookup uses the mode given and no mode looks up nothing");
    const auto jj = map_of({MapRow{insert_only, "jj", "<Esc>", false}});
    const Keys rest = text_keys(U"jjk");
    const auto three = core::rhs_unit_step(jj, VimMapMode::insert, rest);
    expect(three.first == 2U && std::ranges::equal(three.second, Keys{escape()}),
           "the longest left side inside the right side is replaced once");
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

// Emit の単位ごとに、引き直さない分はそのまま、引き直す分は mode のまま rhs_unit_step で 1 段
// 引いた列（モードの変わらない流しで engine に届く鍵・#1276）。
[[nodiscard]] Keys expanded(const KeyMap &map, VimMapMode mode, const KeyMapEmit &emit)
{
    Keys flowed;
    std::size_t begin = 0;
    for (std::size_t unit = 0; unit < emit.ends.size(); ++unit)
    {
        const auto remap = static_cast<std::ptrdiff_t>(emit.remaps.at(unit));
        flowed.insert(flowed.end(), emit.keys.begin() + static_cast<std::ptrdiff_t>(begin),
                      emit.keys.begin() + remap);
        const auto again = rhs_looked_up(
            map, mode,
            Keys(emit.keys.begin() + remap,
                 emit.keys.begin() + static_cast<std::ptrdiff_t>(emit.ends.at(unit))));
        flowed.insert(flowed.end(), again.begin(), again.end());
        begin = emit.ends.at(unit);
    }
    return flowed;
}

// 鍵を 1 つずつ入口に流したときの engine に届く列（待ちは最後に 1 秒の待ちで流す・モードは
// 変わらないとみなす）。
[[nodiscard]] Keys streamed(const KeyMap &map, VimMapMode mode, const Keys &input, bool &bounded)
{
    Keys flowed;
    Keys pending;
    for (const auto &key : input)
    {
        auto step = core::key_map_step(map, mode, pending, key);
        if (auto *emit = std::get_if<KeyMapEmit>(&step))
        {
            const auto keys = expanded(map, mode, *emit);
            flowed.insert(flowed.end(), keys.begin(), keys.end());
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
    const auto rest = expanded(map, mode, core::key_map_flush(map, mode, pending));
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

// 素朴な置き換え（左辺の最も長い一致を先頭から右辺に替え、右辺は引き直さない）。
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

// モードが変わらないときの素朴な模型（#1276）: 左辺の最も長い一致を先頭から右辺に替え、`map` の
// 右辺はもう 1 段だけ replaced で替える（右辺が左辺で始まるならその分は替えない）。
[[nodiscard]] Keys replaced_once(const KeyMap &map, VimMapMode mode, const Keys &input)
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
        at += best->lhs.size();
        if (!best->remap)
        {
            result.insert(result.end(), best->rhs.begin(), best->rhs.end());
            continue;
        }
        const bool keeps = best->rhs.size() >= best->lhs.size() &&
                           std::equal(best->lhs.begin(), best->lhs.end(), best->rhs.begin());
        const auto kept = static_cast<std::ptrdiff_t>(keeps ? best->lhs.size() : 0U);
        result.insert(result.end(), best->rhs.begin(), best->rhs.begin() + kept);
        const auto again = replaced(map, mode, Keys(best->rhs.begin() + kept, best->rhs.end()));
        result.insert(result.end(), again.begin(), again.end());
    }
    return result;
}

// 列の区切りが昇順で、空の区切りが無く、最後が whole か（#1257）。
[[nodiscard]] bool rising_to(const std::vector<std::size_t> &marks, std::size_t whole)
{
    std::size_t begin = 0;
    for (const std::size_t end : marks)
    {
        if (end <= begin)
        {
            return false;
        }
        begin = end;
    }
    return begin == whole;
}

// 単位の区切り（流す鍵の位置と読んだ鍵の位置）が同じ数で、どちらも昇順で最後が列の長さか
// （#1257・#1266）。
// 引き直しの始まりは同じ数で、どれも単位の中か終わり（#1276）。表に当たったかの印も同じ数で、
// 当たらない単位は 1 鍵を読んで 1 鍵を流す（#1312）。
[[nodiscard]] bool whole_units(const KeyMapEmit &emitted, std::size_t read)
{
    if (emitted.reads.size() != emitted.ends.size() ||
        emitted.remaps.size() != emitted.ends.size() ||
        emitted.mapped.size() != emitted.ends.size() ||
        !rising_to(emitted.ends, emitted.keys.size()) || !rising_to(emitted.reads, read))
    {
        return false;
    }
    std::size_t begin = 0;
    std::size_t taken = 0;
    for (std::size_t unit = 0; unit < emitted.ends.size(); ++unit)
    {
        const std::size_t end = emitted.ends.at(unit);
        if (emitted.remaps.at(unit) < begin || emitted.remaps.at(unit) > end)
        {
            return false;
        }
        if (!emitted.mapped.at(unit) && (end - begin != 1U || emitted.reads.at(unit) - taken != 1U))
        {
            return false;
        }
        begin = end;
        taken = emitted.reads.at(unit);
    }
    return true;
}

void verify_key_map_property()
{
    Random random{0x1058'0A11'0B0E'5EEDULL};
    std::size_t agreed = 0;
    std::size_t ordered = 0;
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
        const auto whole = core::key_map_flush(map, mode, input);
        agreed += flowed == expanded(map, mode, whole) ? 1U : 0U;
        ordered += static_cast<std::size_t>(whole_units(whole, input.size()));
        // noremap だけの表では replaced_once は replaced と同じ列になる。
        matched += flowed == replaced_once(map, mode, input) ? 1U : 0U;
    }
    expect(agreed == property_cases,
           "keys flowed one at a time equal the whole input replaced at once");
    expect(ordered == property_cases,
           "the units rise strictly, end at the last key flowed and read, and remap inside");
    expect(matched == property_cases,
           "the flowed keys replace the longest left side from the start and a map right side "
           "once more");
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

// `.` の記録は engine に届いた鍵（決定 7）・press_vim_keys は表を引かない（決定 3）。マクロの
// 録画と `@` は #1312（追補 32〜35・verify_recorded_typed_keys ほか）。
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

// Vim の 100 行の本文で start（1 行目からの移動・空なら 1 行目）の後に keys を打った後の
// キャレットの行と本文。timeout なら最後に 1 秒の待ちを流す。
[[nodiscard]] std::pair<std::size_t, std::string> hundred_lines_from(std::vector<MapRow> rows,
                                                                     std::string_view start,
                                                                     std::string_view keys,
                                                                     bool timeout)
{
    Editing editing(mapped_settings(std::move(rows)));
    std::string text;
    for (std::size_t line = 1; line <= 100U; ++line)
    {
        text += std::to_string(line) + (line < 100U ? "\n" : "");
    }
    // 末尾に改行を置かない（置くと Nib の本文は 101 行目の空行を持ち `44%` が 45 行目になる）。
    open_vim_document(editing, std::move(text));
    auto &controller = editing.controller();
    vim_replay(controller, start);
    vim_replay(controller, keys);
    if (timeout)
    {
        static_cast<void>(controller.apply(application::MapTimeout{}));
    }
    return {controller.frame().caret.position.line.value, whole_vim_body(controller)};
}

// 50G から keys を打った後のキャレットの行と本文。
[[nodiscard]] std::pair<std::size_t, std::string>
hundred_lines_after(std::vector<MapRow> rows, std::string_view keys, bool timeout)
{
    return hundred_lines_from(std::move(rows), "50G", keys, timeout);
}

[[nodiscard]] std::size_t hundred_lines_caret(std::vector<MapRow> rows, std::string_view keys,
                                              bool timeout)
{
    return hundred_lines_after(std::move(rows), keys, timeout).first;
}

// 右辺の途中の鍵が失敗したら右辺の残りを捨て、打った鍵は流す（Vim の beep_flush の
// FLUSH_MINIMAL・追補 19・#1257）。期待値は Vim 9.1 の実測（100 行の本文・feedkeys の 't'）:
// `x` → 1 行目・`xj` → 2・`qj`（`qz` の待ちの後の j）→ 2・`xw`（w は 3j）→ 4・`q` の待ちの
// タイムアウト → 1。失敗しない右辺は今までどおり `y` → 44・`yj` → 45。
[[nodiscard]] std::size_t mapped_line(std::string_view keys, bool timeout)
{
    return hundred_lines_caret(
        {MapRow{normal_only, "x", "gg<C-b>44%", true}, MapRow{normal_only, "y", "gg44%", true},
         MapRow{normal_only, "q", "gg<C-b>44%", true}, MapRow{normal_only, "qz", "G", true},
         MapRow{normal_only, "w", "3j", true}},
        keys, timeout);
}

void verify_failed_expansion()
{
    expect(mapped_line("x", false) == 1U, "<C-b> failing at the top drops 44% of the right side");
    expect(mapped_line("xj", false) == 2U, "a key typed after the failed right side still runs");
    expect(mapped_line("qj", false) == 2U,
           "the key that ended a wait runs after the failed right side");
    expect(mapped_line("xw", false) == 4U, "the next mapped key still expands and runs");
    expect(mapped_line("q", true) == 1U, "the timeout flush drops the rest of a failed right side");
    expect(mapped_line("y", false) == 44U, "a right side that does not fail runs to its end");
    expect(mapped_line("yj", false) == 45U, "a key typed after a whole right side runs");
}

// 前の単位がモードを変えたら、後の単位と待ちは今のモードの表で引く（Vim 9.1 の実測・
// `feedkeys(…, 'tx')`・#1266）。`nmap x v` の後の `j` は `vmap j` で引かれる。
void verify_unit_mode()
{
    const MapRow visual_x{normal_only, "x", "v", true};
    const MapRow wait_x{normal_only, "xy", "G", true};
    const MapRow visual_j{visual_set, "j", "3k<Esc>", true};
    expect(hundred_lines_caret({visual_x, wait_x, visual_j}, "xj", false) == 47U,
           "a key after a right side that enters VISUAL uses the vmap");
    expect(hundred_lines_caret({visual_x, MapRow{normal_only, "xyz", "G", true},
                                MapRow{visual_set, "y", "3k<Esc>", true}},
                               "xy", true) == 47U,
           "the timeout flush looks up the later key in the new mode");
    expect(hundred_lines_caret({MapRow{normal_only, "x", "2j", true}, wait_x, visual_j}, "xj",
                               false) == 53U,
           "a right side that keeps NORMAL keeps the nmap table for the later key");
    const MapRow append{normal_only, "q", "A", true};
    const MapRow wait_q{normal_only, "qz", "G", true};
    expect(hundred_lines_caret({append, wait_q, MapRow{insert_only, "j", "<Esc>3k", true}}, "qj",
                               false) == 47U,
           "a key after a right side that enters INSERT uses the imap");
    expect(hundred_lines_caret({append, wait_q}, "qj<Esc>", false) == 50U,
           "without an imap the later key is typed into the text");
    const MapRow visual_jk{visual_set, "jk", "5k<Esc>", true};
    expect(hundred_lines_caret({visual_x, wait_x, visual_jk, visual_j}, "xj", false) == 50U,
           "a later key that starts a vmap left side waits in the new mode");
    expect(hundred_lines_caret({visual_x, wait_x, visual_jk, visual_j}, "xj", true) == 47U,
           "the new wait flushes with the vmap on the timeout");
    expect(hundred_lines_caret({visual_x, wait_x, visual_jk, visual_j}, "xjk", false) == 45U,
           "the new wait completes the longer vmap");
    expect(hundred_lines_caret({MapRow{normal_only, "x", "f", true},
                                MapRow{normal_only, "x0z", "G", true},
                                MapRow{normal_only, "k", "3j", true}},
                               "x0k", false) == 53U,
           "after the argument of f the later key is looked up in NORMAL again");
}

// 右辺の中の鍵は流す時点のモードの表で 1 段だけ引き直す（Vim 9.1 の実測・100 行の本文・`50G`
// から・`feedkeys(…, 'tx')`・`D:/NeNeNib/scripts/20261110/1276/probe.vim` の B と同じ置き場の
// `1276-I8/probe.vim` の L C・#1276・ADR 0176 の追補 26）。
void verify_rhs_mode()
{
    const MapRow visual_x{normal_only, "x", "vj", true};
    const MapRow visual_k{visual_set, "j", "k", true};
    expect(hundred_lines_caret({visual_x, visual_k}, "x", false) == 49U,
           "B1: the j of a right side that entered VISUAL uses the vmap");
    expect(hundred_lines_caret({MapRow{normal_only, "x", "vjj", true}, visual_k}, "x", false) ==
               48U,
           "B3: every later key of the right side uses the vmap");
    expect(hundred_lines_caret({visual_x, MapRow{visual_set, "j", "k", false}}, "x", false) == 49U,
           "B7: a vnoremap row is looked up the same way");
    expect(hundred_lines_caret({visual_x, visual_k}, "xj", false) == 48U,
           "B14: the right side and the typed key both use the vmap");
    expect(hundred_lines_caret({MapRow{normal_only, "x", "vj", false}, visual_k}, "x", false) ==
               51U,
           "B2: an nnoremap right side is not looked up in VISUAL either");
    expect(hundred_lines_caret({visual_x, MapRow{normal_only, "j", "3j", true}}, "x", false) == 51U,
           "B6: an nmap does not apply to the right side once it is in VISUAL");
    expect(hundred_lines_caret({MapRow{normal_only, "x", "v<Esc>j", true}, visual_k}, "x", false) ==
               51U,
           "B15: a right side back in NORMAL uses the nmap table again");
    // 限界（追補 28）: 引き直した右辺はもう引かない（P3・Vim 52）。右辺の内側の引き直しは右辺の
    // 終わりをまたいで待たない（P8b・`xj` を 1 回の typeahead で Vim 100）。
    expect(hundred_lines_caret({visual_x, visual_k, MapRow{visual_set, "k", "2j", false}}, "x",
                               false) == 49U,
           "P3: the right side looked up once is not looked up again");
    expect(hundred_lines_caret({visual_x, visual_k, MapRow{visual_set, "jj", "G", true}}, "xj",
                               true) == 48U,
           "P8b: the look-up inside a right side does not wait across its end");
}

// 右辺が INSERT・OPERATOR-PENDING・入力行・引数待ちに入った後の鍵（#1276・同じ実測）。
void verify_rhs_other_modes()
{
    const auto inserted = hundred_lines_after(
        {MapRow{normal_only, "x", "ij<Esc>", true}, MapRow{insert_only, "j", "Z", true}}, "x",
        false);
    expect(inserted.first == 50U && inserted.second.contains("\n49\nZ50\n51\n"),
           "B8: the j of a right side that entered INSERT uses the imap");
    const auto removed = hundred_lines_after(
        {MapRow{normal_only, "x", "dj", true}, MapRow{pending_only, "j", "k", true}}, "x", false);
    expect(removed.first == 49U && removed.second.contains("\n48\n51\n52\n"),
           "B9: the motion of a right side after an operator uses the omap");
    const auto selected = hundred_lines_after(
        {MapRow{normal_only, "x", "vjd", true}, MapRow{visual_set, "j", "k", true}}, "x", false);
    expect(selected.first == 49U && selected.second.contains("\n48\n0\n51\n"),
           "C3: the operator after the vmap motion runs on the VISUAL range");
    const auto replaced_z = hundred_lines_after({MapRow{normal_only, "x", "rZk", true},
                                                 MapRow{normal_only, "Z", "G", true},
                                                 MapRow{normal_only, "k", "3j", true}},
                                                "x", false);
    expect(replaced_z.first == 53U && replaced_z.second.contains("\n49\nZ0\n51\n"),
           "C1: the argument of r in a right side is not looked up and the key after it is");
    const MapRow down_5{normal_only, "5", "3j", true};
    expect(hundred_lines_caret({MapRow{normal_only, "x", ":5<CR>", true}, down_5}, "x", false) ==
               5U,
           "L1a: the keys of a right side in the command line are not looked up");
    expect(hundred_lines_caret(
               {MapRow{normal_only, "q", ":", true}, MapRow{normal_only, "qz", "G", true}, down_5},
               "q5<CR>", false) == 5U,
           "L1b: the typed key after a right side that opened the command line is not looked up");
}

// 1 行目から keys を打った後のキャレットの行。
[[nodiscard]] std::size_t top_caret(std::vector<MapRow> rows, std::string_view keys)
{
    return hundred_lines_from(std::move(rows), "", keys, false).first;
}

[[nodiscard]] std::size_t line_count(std::string_view body)
{
    return static_cast<std::size_t>(std::ranges::count(body, '\n')) + 1U;
}

// 回数の続きの `0` は表を引かない（Vim 9.1 の no_zero_mapping の実測・100 行の本文・
// `feedkeys(…, 'tx')`・`D:/NeNeNib/scripts/20261110/1311/probe.vim` の Z・#1311・ADR 0176 の
// 追補 30）。回数の先頭の `0`・オペレータの後の `0`・回数が終わった後の `0` は引く。
void verify_zero_in_count_keys()
{
    const MapRow zero_end{normal_only, "0", "G", true};
    expect(top_caret({zero_end}, "10j") == 11U, "Z1: the 0 of 10j is a digit");
    expect(top_caret({zero_end}, "50G") == 50U, "Z2: nmap 0 G leaves 50G on line 50");
    expect(hundred_lines_caret({zero_end}, "0", false) == 100U, "Z3: a 0 alone uses the nmap");
    const MapRow visual_zero{visual_set, "0", "G", true};
    expect(top_caret({visual_zero}, "v10j<Esc>") == 11U &&
               top_caret({visual_zero}, "V10j<Esc>") == 11U,
           "Z4 Z5: the 0 of a VISUAL count is a digit");
    expect(hundred_lines_caret({visual_zero}, "v0<Esc>", false) == 100U,
           "Z6: a 0 alone in VISUAL uses the vmap");
    const MapRow pending_zero{pending_only, "0", "G", true};
    const auto counted = hundred_lines_from({pending_zero}, "", "d10j", false);
    expect(counted.first == 1U && line_count(counted.second) == 89U &&
               counted.second.starts_with("12\n13\n"),
           "Z7: the 0 of d10j is a digit and 11 lines go");
    const auto alone = hundred_lines_after({pending_zero}, "d0", false);
    expect(alone.first == 49U && line_count(alone.second) == 49U,
           "Z8: d0 uses the omap and deletes to the end");
    const auto after_count = hundred_lines_after({pending_zero}, "2d0", false);
    expect(after_count.first == 2U && line_count(after_count.second) == 51U &&
               after_count.second.starts_with("1\n51\n"),
           "Z9: the 0 after 2d starts no count and uses the omap");
    expect(top_caret({zero_end}, "\"a10j") == 11U, "Z10: the 0 of a count after \"a is a digit");
    // 限界（追補 31）: Vim は `"a` の後で回数を数え直す（Z11・`5"a0` は `0` を引いて 5 行目）。
    // Nib の engine は `"a` の後も回数を続ける（`5"a0` は回数 50）ので、表もそれに従って引かない。
    expect(hundred_lines_caret({zero_end}, "5\"a0", false) == 50U,
           "Z11: the 0 after 5\"a follows the engine count and is not looked up");
    expect(top_caret({MapRow{normal_only, "x", "10j", true}, zero_end}, "x") == 11U,
           "Z12: the 0 of a count in a right side is not looked up");
    const MapRow zero_a{normal_only, "0a", "G", true};
    const auto appended = hundred_lines_from({zero_a}, "", "10aZ<Esc>", false);
    expect(appended.second.starts_with("1ZZZZZZZZZZ\n"),
           "Z13: the 0 of a count does not start the left side 0a");
    expect(top_caret({zero_a}, "0a") == 100U, "Z14: 0a without a count uses the nmap");
    expect(top_caret({MapRow{normal_only, "10", "G", true}}, "10") == 100U,
           "Z15: a left side that starts with a digit before the count is mapped");
    expect(top_caret({MapRow{normal_only, "5a", "x", true}, MapRow{normal_only, "0", "gg", true}},
                     "50j") == 51U,
           "Z16: the 0 after a waited 5 that became a count is a digit");
    expect(top_caret({MapRow{normal_only, "2a", "x", true}, MapRow{normal_only, "0", "gg", true}},
                     "320G") == 100U,
           "Z17: the 0 after a waited digit inside the count is a digit");
    expect(hundred_lines_caret({zero_end}, "g0", false) == 50U,
           "Z18: the argument of g is not mapped");
    const auto inserted =
        hundred_lines_from({MapRow{insert_only, "0", "Z", true}}, "", "3i0<Esc>", false);
    expect(inserted.second.starts_with("ZZZ1\n"), "Z19: INSERT maps the 0 after a count");
    const auto removed = hundred_lines_after({zero_end}, "5x0", false);
    expect(removed.first == 100U && removed.second.contains("\n49\n\n51\n"),
           "Z20: the 0 after the count is used up uses the nmap");
    const auto replaced = hundred_lines_from({zero_end}, "", "1r0", false);
    expect(replaced.first == 1U && replaced.second.starts_with("0\n2\n"),
           "Z22: the argument of r is not mapped");
    const auto deleted = hundred_lines_after({zero_end}, "d20j", false);
    expect(deleted.first == 50U && line_count(deleted.second) == 79U,
           "Z23: the nmap does not apply to the 0 of d20j");
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
    Editing closing(insert_escape());
    open_vim_document(closing, "abc\n");
    vim_replay(closing.controller(), "ij");
    const auto ended =
        closing.controller().apply(application::EndSession{application::SessionEnd::window_closed});
    expect(shown_body(closing.controller()) == "abc\n" && ended.map_timer == MapTimer::stop &&
               !closing.controller().apply(application::MapTimeout{}).repaint,
           "EndSession drops the waiting j instead of typing it");
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

// ---- マクロの録画は打った鍵・`@` は今の表で引く（#1312・ADR 0176 の追補 33）----

using application::EditorController;

// Vim の 100 行の本文（1 行目だけ first・末尾に改行を置かない）。
[[nodiscard]] std::string hundred_text(std::string_view first)
{
    std::string text{first};
    for (std::size_t line = 2; line <= 100U; ++line)
    {
        text += "\n" + std::to_string(line);
    }
    return text;
}

// レジスタの本文を Vim の strtrans の形で（`<Esc>` は `^[`）。
[[nodiscard]] std::string shown_register(const core::VimState &state, char32_t name)
{
    const auto &text =
        state.registers.registers.at(core::vim_register_index(name).value_or(0)).value().text;
    std::string shown;
    for (const char letter : text)
    {
        shown += letter == '\x1b' ? std::string{"^["} : std::string(1, letter);
    }
    return shown;
}

// 本文の line 行目（1 から）。無ければ空。
[[nodiscard]] std::string body_line(std::string_view body, std::size_t line)
{
    std::size_t begin = 0;
    for (std::size_t at = 1; at < line; ++at)
    {
        const auto next = body.find('\n', begin);
        if (next == std::string_view::npos)
        {
            return {};
        }
        begin = next + 1;
    }
    return std::string{body.substr(begin, body.find('\n', begin) - begin)};
}

// 1 手。`:` で始まれば Ex、`=a:` で始まればレジスタ a へ本文（鍵の記法）を置き、`~` は 1 秒の
// 待ちの終わり、ほかは打つ鍵（窓の入口 press_vim_key・`map` の表を引く）。
void probe_step(EditorController &controller, std::string_view step)
{
    if (step.starts_with(':'))
    {
        static_cast<void>(run_ex(controller, std::string{step.substr(1)}));
        return;
    }
    if (step.starts_with('='))
    {
        static_cast<void>(controller.apply(application::StoreVimRegister{
            step.at(1), core::VimRegister{core::vim_register_text(vim_keys_of(step.substr(3))),
                                          core::VimRegisterKind::characters}}));
        return;
    }
    if (step == "~")
    {
        static_cast<void>(controller.apply(application::MapTimeout{}));
        return;
    }
    vim_replay(controller, step);
}

// Vim の probe（`D:\NeNeNib\scripts\20261110\1312\probe.vim`）の Log と同じ形の要約。
[[nodiscard]] std::string probe_summary(EditorController &controller)
{
    const std::string body = whole_vim_body(controller);
    const auto lines = static_cast<std::size_t>(std::ranges::count(body, '\n')) + 1U;
    return "line=" + std::to_string(controller.frame().caret.position.line.value) +
           " lines=" + std::to_string(lines) + " t50=" + body_line(body, 50U) +
           " t51=" + body_line(body, 51U) + " t52=" + body_line(body, 52U) +
           " rega=" + shown_register(controller.vim_state(), U'a') +
           " regb=" + shown_register(controller.vim_state(), U'b');
}

// 表 rows・1 行目 first の 100 行で steps を順に流し、手ごとの要約を返す。
[[nodiscard]] std::vector<std::string> probed(std::vector<MapRow> rows, std::string_view first,
                                              std::initializer_list<std::string_view> steps)
{
    Editing editing(mapped_settings(std::move(rows)));
    open_vim_document(editing, hundred_text(first));
    auto &controller = editing.controller();
    std::vector<std::string> summaries;
    for (const std::string_view step : steps)
    {
        probe_step(controller, step);
        summaries.push_back(probe_summary(controller));
    }
    return summaries;
}

[[nodiscard]] std::string probed_last(std::vector<MapRow> rows,
                                      std::initializer_list<std::string_view> steps)
{
    return probed(std::move(rows), "1", steps).back();
}

[[nodiscard]] std::string vim_line(std::size_t line, std::string_view registers)
{
    return "line=" + std::to_string(line) + " lines=100 t50=50 t51=51 t52=52 " +
           std::string{registers};
}

// 録るのは打った左辺・止める `q` と始める `qa` を右辺が流したら左辺ごと録らない（R1 R4a R6a
// R14 R15 R16 R17a R18 R20）。期待値は Vim 9.1 の実測（`out.txt`・`feedkeys(…, 'tx')`）。
void verify_recorded_typed_keys()
{
    const MapRow x_j{normal_only, "x", "j", true};
    expect(probed_last({x_j}, {"50Gqaxq"}) == vim_line(51U, "rega=x regb="),
           "R1: qaxq with nmap x j records x");
    expect(probed_last({x_j, MapRow{normal_only, "j", "k", true}}, {"50Gqaxq"}) ==
               vim_line(49U, "rega=x regb="),
           "R4a: the right side remapped once still records the typed x");
    expect(probed_last({MapRow{normal_only, "jj", "G", true}}, {"50Gqajjq"}) ==
               vim_line(100U, "rega=jj regb="),
           "R6a: a two key left side records both typed keys");
    expect(probed_last({MapRow{normal_only, ",", "q", true}}, {"50Gqaj,"}) ==
               vim_line(51U, "rega=j regb="),
           "R14: the mapped key whose right side stops recording is not recorded");
    expect(probed_last({MapRow{normal_only, "Z", "qa", true}}, {"50GZjq"}) ==
               vim_line(51U, "rega=j regb="),
           "R15: the mapped key whose right side starts recording is not recorded");
    expect(probed_last({MapRow{normal_only, "x", "3j", true}}, {"50Gqaxq"}) ==
               vim_line(53U, "rega=x regb="),
           "R16: the right side 3j is not recorded");
    expect(probed_last({MapRow{insert_only, "j", "Z", true}}, {"50Gqaij<Esc>q"}) ==
               "line=50 lines=100 t50=Z50 t51=51 t52=52 rega=ij^[ regb=",
           "R17a: an INSERT map records the typed j");
    expect(probed_last({x_j}, {"=a:x", "50Gqb@aq"}) == vim_line(51U, "rega=x regb=@a"),
           "R18: the replayed keys are not recorded");
    expect(probed_last({MapRow{normal_only, "q", "j", true}}, {"50Gqa"}) ==
               vim_line(51U, "rega= regb="),
           "R20: a mapped q is j and cannot start recording");
}

// 右辺が録画を止める・始める形（R21〜R28）。左辺が録画から消えるのは右辺の最初の鍵が録画を
// 止めたときだけで、右辺の途中で止めたら左辺は残る（Vim の get_recorded の last_recorded_len・
// ADR 0176 の追補 32）。右辺が録画を始めたら、どの位置でも左辺は録らない。期待値は Vim 9.1 の実測
// （`D:\NeNeNib\scripts\20261110\i12b\probe.vim`・`feedkeys(…, 'tx')`）。
void verify_recording_edges()
{
    expect(probed_last({MapRow{normal_only, "Z", "jq", true}}, {"50GqajZ"}) ==
               vim_line(52U, "rega=jZ regb="),
           "R21: a right side that stops recording after j keeps the typed Z");
    expect(probed_last({MapRow{normal_only, "Z", "qqb", true}}, {"50GqajZkq"}) ==
               vim_line(50U, "rega=j regb=k"),
           "R22: the first q drops Z and the next qb records only the typed k");
    expect(probed_last({MapRow{normal_only, "Z", "jqqb", true}}, {"50GqajZkq"}) ==
               vim_line(51U, "rega=jZ regb=k"),
           "R23: a later q keeps Z in a and qb records only the typed k");
    expect(probed_last({MapRow{normal_only, "Z", "Yq", true}, MapRow{normal_only, "Y", "j", true}},
                       {"50GqajZ"}) == vim_line(52U, "rega=jZ regb="),
           "R24: a remapped first key j keeps Z");
    expect(probed_last({MapRow{normal_only, "Z", "Y", true}, MapRow{normal_only, "Y", "q", true}},
                       {"50GqajZ"}) == vim_line(51U, "rega=j regb="),
           "R25: a remapped first key q drops Z");
    expect(probed_last({MapRow{normal_only, "Z", "qa", true}}, {"50GZjjq"}) ==
               vim_line(52U, "rega=jj regb="),
           "R26: a right side qa starts recording without Z");
    expect(probed_last({MapRow{normal_only, "Z", "jqa", true}}, {"50GZjq"}) ==
               vim_line(52U, "rega=j regb="),
           "R27: a right side that starts recording after j does not record Z");
    expect(probed_last({MapRow{normal_only, "Z", "qajq", true}}, {"50GZ"}) ==
               vim_line(51U, "rega= regb="),
           "R28: a right side that starts and stops recording leaves a empty");
}

// `@a` `@@` `[count]@a` と本文から作ったレジスタは流す時点の表で引く（R2 R3 R4b R6b R7 R11 R12
// R13 R17c R19）。`.` は引かない（R17b）。
void verify_macro_replay_maps()
{
    const MapRow x_j{normal_only, "x", "j", true};
    expect(probed_last({x_j}, {"50Gqaxq", ":nunmap x", ":nmap x k", "@a"}) ==
               vim_line(50U, "rega=x regb="),
           "R2: @a looks up the table of the replay");
    expect(probed_last({x_j}, {"50Gqaxq", ":nunmap x", "@a"}) ==
               "line=51 lines=100 t50=50 t51=1 t52=52 rega=x regb=",
           "R3: without the row @a runs the recorded x");
    expect(probed_last({x_j, MapRow{normal_only, "j", "k", true}},
                       {"50Gqaxq", ":nunmap x", ":nnoremap x j", "@a"}) ==
               vim_line(50U, "rega=x regb="),
           "R4b: a noremap row at the replay is not looked up again");
    expect(probed_last({MapRow{normal_only, "jj", "G", true}},
                       {"50Gqajjq", ":nunmap jj", "50G@a"}) == vim_line(52U, "rega=jj regb="),
           "R6b: the recorded jj replays as two j without the row");
    expect(probed({x_j}, "x", {"gg\"ay$50G@a"}).back() == vim_line(51U, "rega=x regb="),
           "R7: a register made by yank is looked up too");
    expect(probed_last({x_j}, {"=a:x", "50G@a", ":nunmap x", ":nmap x k", "@@"}) ==
               vim_line(50U, "rega=x regb="),
           "R11: @@ looks up the table of its own replay");
    expect(probed_last({x_j}, {"=a:x", "50G3@a"}) == vim_line(53U, "rega=x regb="),
           "R12: 3@a looks up every round");
    expect(probed_last({MapRow{normal_only, "x", "@b", true}}, {"=b:jj", "=a:x", "50G@a"}) ==
               vim_line(52U, "rega=x regb=jj"),
           "R13: a right side @b inside @a runs b");
    const auto insert = probed({MapRow{insert_only, "j", "Z", true}}, "1",
                               {"50Gqaij<Esc>q", ":iunmap j", ":imap j Y", "j.", "j@a"});
    expect(insert.at(3) == "line=51 lines=100 t50=Z50 t51=Z51 t52=52 rega=ij^[ regb=",
           "R17b: . repeats the replaced Z without the table");
    expect(insert.at(4) == "line=52 lines=100 t50=Z50 t51=Z51 t52=Y52 rega=ij^[ regb=",
           "R17c: @a looks up imap j Y");
    expect(probed_last({MapRow{normal_only, "x", "@a", true}}, {"=a:jx", "90G@a"}) ==
               vim_line(100U, "rega=jx regb="),
           "R19: a recursive macro through a map ends at the failing j");
}

// 待ちの左辺は時間切れなら 1 つずつ録り、再生の中の待ちは列が尽きたら待たずに流す（R5a R5b）。
// レジスタの末尾の `j` と後から打った `j` はまたがない（R5c・Vim は 100・追補 34 の限界 (d)）。
void verify_macro_waiting_keys()
{
    const MapRow jj_g{normal_only, "jj", "G", true};
    const auto timed = probed({jj_g}, "1", {"50Gqaj", "~", "q", "@a"});
    expect(timed.at(2) == vim_line(51U, "rega=j regb="), "R5a: the timed out j is recorded");
    expect(timed.at(3) == vim_line(52U, "rega=j regb="),
           "R5b: the replayed j waits for nothing at the end of the register");
    expect(probed_last({jj_g}, {"=a:j", "50G@aj", "~"}) == vim_line(52U, "rega=j regb="),
           "R5c: the register's j and a typed j do not join (Vim 100)");
}

// 表が空の editor と、どの行にも当たらない表の editor で、同じ鍵の列（録画・再生・`.`・INSERT）の
// 本文とキャレットの行と列とレジスタ a b が一致する（固定 seed）。second は再生の中の待ちの回数。
[[nodiscard]] std::pair<std::string, std::size_t> macro_run(std::vector<MapRow> rows,
                                                            std::span<const std::string> words)
{
    Editing editing(mapped_settings(std::move(rows)));
    open_vim_document(editing, "abc def\nghi jkl\nmno pqr\nstu vwx\n");
    auto &controller = editing.controller();
    for (const std::string &word : words)
    {
        vim_replay(controller, word);
    }
    static_cast<void>(controller.apply(application::MapTimeout{}));
    const auto caret = controller.frame().caret.position;
    return {whole_vim_body(controller) + "|" + std::to_string(caret.line.value) + ":" +
                std::to_string(caret.column.value) + "|" +
                shown_register(controller.vim_state(), U'a') + "|" +
                shown_register(controller.vim_state(), U'b'),
            controller.replay_waits()};
}

void verify_unmatched_table_property()
{
    constexpr std::array<std::string_view, 16> words{"qa", "qb",      "q",  "x",  "j",   "k",
                                                     "w",  "b",       "@a", "@b", "2@a", "@@",
                                                     ".",  "iy<Esc>", "u",  "dd"};
    // `xQ` と `yQ` は語の頭の鍵で始まって完結しない左辺で、打った `x` `y` と再生の中の `x` を
    // 待たせて次の鍵（列の終わり）で流す（打った鍵の待ちと時間切れ・replay_pending_ と flush
    // の道）。
    const std::vector<MapRow> unmatched{
        MapRow{normal_only, "Q", "dd", true},     MapRow{normal_only, "Zx", "G", true},
        MapRow{insert_only, "Q", "<Esc>", false}, MapRow{visual_set, "Q", "d", true},
        MapRow{pending_only, "Q", "w", true},     MapRow{normal_only, "xQ", "G", true},
        MapRow{insert_only, "yQ", "Z", true}};
    Random random{1312U};
    constexpr std::size_t cases = 200U;
    std::size_t agreed = 0;
    std::size_t waits = 0;
    for (std::size_t round = 0; round < cases; ++round)
    {
        std::vector<std::string> picked;
        const auto count = 4U + random() % 12U;
        for (std::size_t index = 0; index < count; ++index)
        {
            picked.emplace_back(words.at(random() % words.size()));
        }
        const auto [plain, plain_waits] = macro_run({}, picked);
        const auto [mapped, mapped_waits] = macro_run(unmatched, picked);
        expect(plain_waits == 0U, "an empty table never waits inside a replay");
        agreed += plain == mapped ? 1U : 0U;
        waits += mapped_waits;
    }
    expect(agreed == cases,
           "an empty table and a table that never matches record and replay the same keys");
    // 再生の中で `x` を待った回数（固定 seed 1312 の 200 件の数え値・待ちと flush の道を通る）。
    expect(waits == 103U, "the replays wait on x and flush it the expected times");
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
    verify_zero_in_count();
    verify_steps();
    verify_raw_right_sides();
    verify_key_map_property();
    verify_insert_escape();
    verify_mismatch_and_timeout();
    verify_remap_depth();
    verify_leader_change();
    verify_mode_tables();
    verify_ordinary_mode();
    verify_records_and_replay();
    verify_failed_expansion();
    verify_unit_mode();
    verify_rhs_mode();
    verify_rhs_other_modes();
    verify_zero_in_count_keys();
    verify_flush_before_intents();
    verify_walk_and_size();
    verify_ex_commands();
    verify_ex_parsing();
    verify_recorded_typed_keys();
    verify_recording_edges();
    verify_macro_replay_maps();
    verify_macro_waiting_keys();
    verify_unmatched_table_property();
}
} // namespace nenenib::tests
