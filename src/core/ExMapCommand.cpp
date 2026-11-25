#include "ExMapCommand.hpp"

#include "ExFailure.hpp"
#include "ExMapName.hpp"
#include "KeyMap.hpp"
#include "KeyMapFailure.hpp"
#include "MapRow.hpp"
#include "Offset.hpp"
#include "Utf8.hpp"
#include "VimKeyNotation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
constexpr VimModeSet normal_modes{.normal = true};
constexpr VimModeSet insert_modes{.insert = true};
constexpr VimModeSet visual_modes{.visual = true};
constexpr VimModeSet visual_only_modes{.visual_only = true};
constexpr VimModeSet pending_modes{.operator_pending = true};
// `:map` `:noremap` `:unmap` は NORMAL・VISUAL・OPERATOR-PENDING（Vim と同じく INSERT
// を含まない）。
constexpr VimModeSet plain_modes{.normal = true, .visual = true, .operator_pending = true};

// 命令の名前の表（CPP-012）。省略は Vim 9.1 の最短の形から完全な形までの前方一致。`:ma` は mark
// なので `:map` は省略できない。`:no` は noremap（`:noh` は先に nohlsearch が受ける）。
constexpr std::array<ExMapName, 18> map_names{{
    {"map", 3, ExMapVerb::add, plain_modes, true},
    {"noremap", 2, ExMapVerb::add, plain_modes, false},
    {"nmap", 2, ExMapVerb::add, normal_modes, true},
    {"nnoremap", 2, ExMapVerb::add, normal_modes, false},
    {"imap", 2, ExMapVerb::add, insert_modes, true},
    {"inoremap", 3, ExMapVerb::add, insert_modes, false},
    {"vmap", 2, ExMapVerb::add, visual_modes, true},
    {"vnoremap", 2, ExMapVerb::add, visual_modes, false},
    {"xmap", 2, ExMapVerb::add, visual_only_modes, true},
    {"xnoremap", 2, ExMapVerb::add, visual_only_modes, false},
    {"omap", 2, ExMapVerb::add, pending_modes, true},
    {"onoremap", 3, ExMapVerb::add, pending_modes, false},
    {"unmap", 3, ExMapVerb::remove, plain_modes, true},
    {"nunmap", 3, ExMapVerb::remove, normal_modes, true},
    {"iunmap", 2, ExMapVerb::remove, insert_modes, true},
    {"vunmap", 2, ExMapVerb::remove, visual_modes, true},
    {"xunmap", 2, ExMapVerb::remove, visual_only_modes, true},
    {"ounmap", 2, ExMapVerb::remove, pending_modes, true},
}};

constexpr std::string_view let_name = "let";
constexpr std::string_view leader_variable = "mapleader";
constexpr std::string_view leader_spelling = "<Leader>";
constexpr std::string_view leader_canonical = "<leader>";

// 候補（完全な形・名前の表と同じ順）と `let mapleader=`。
constexpr std::array<std::string_view, 19> candidates{
    "map",      "noremap", "nmap",     "nnoremap", "imap",          "inoremap", "vmap",
    "vnoremap", "xmap",    "xnoremap", "omap",     "onoremap",      "unmap",    "nunmap",
    "iunmap",   "vunmap",  "xunmap",   "ounmap",   "let mapleader="};

using Evaluated = std::expected<ExResult, ExEvaluationFailure>;

[[nodiscard]] std::string_view trimmed(std::string_view text) noexcept
{
    const auto first = text.find_first_not_of(' ');
    if (first == std::string_view::npos)
    {
        return {};
    }
    return text.substr(first, text.find_last_not_of(' ') - first + 1);
}

[[nodiscard]] bool ascii_letter(char letter) noexcept
{
    return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z');
}

[[nodiscard]] bool ascii_digit(char letter) noexcept
{
    return letter >= '0' && letter <= '9';
}

[[nodiscard]] std::size_t leading(std::string_view text, bool (*accepts)(char) noexcept) noexcept
{
    std::size_t length = 0;
    while (length < text.size() && accepts(text.at(length)))
    {
        ++length;
    }
    return length;
}

[[nodiscard]] Evaluated map_result(ExMapRequest request, std::string_view name)
{
    ExResult result{std::nullopt, std::nullopt, std::nullopt, DisplayText::parse(name).value()};
    result.map = std::move(request);
    return result;
}

// `:nmap {lhs} {rhs}`。引数なしと左辺だけは一覧（Vim と同じ）。
[[nodiscard]] Evaluated added(const ExMapName &row, std::string_view argument)
{
    const auto space = argument.find(' ');
    const auto lhs = argument.substr(0, space);
    const auto rhs =
        space == std::string_view::npos ? std::string_view{} : trimmed(argument.substr(space + 1));
    const ExMapVerb verb = rhs.empty() ? ExMapVerb::list : ExMapVerb::add;
    return map_result(ExMapRequest{verb, row.modes, std::string(lhs), std::string(rhs), row.remap},
                      row.name);
}

// `:nunmap {lhs}`。左辺が無い・2 つ以上の語は E474。
[[nodiscard]] Evaluated removed(const ExMapName &row, std::string_view argument)
{
    if (argument.empty() || argument.find(' ') != std::string_view::npos)
    {
        return std::unexpected(ExFailure::invalid_argument);
    }
    return map_result(ExMapRequest{ExMapVerb::remove, row.modes, std::string(argument), {}, true},
                      row.name);
}

// Vim の文字列の 1 字の鍵の記法（Vim 9.1 の keytrans と同じ名前のうち Nib の表にある綴り・
// #2018）。Tab・改行・CR・ESC・空白は名前、ほかの C0 の 0x01〜0x1A は `<C-a>` 〜 `<C-z>`
// （表に無ければ読みで断る）、NUL と 0x1C〜0x1F と DEL と code point でない値は Nib の表に名前の
// 無い形で、読みで断る。
// `<` は 1 字の値なら `<` のまま、並びの中では `<lt>`（並べた記法が 1 鍵の名前に化けない）。
constexpr std::array<std::pair<char32_t, std::string_view>, 6> character_names{{
    {U'\t', "<Tab>"},
    {U'\n', "<NL>"},
    {U'\r', "<CR>"},
    {U'\x1b', "<Esc>"},
    {U' ', "<Space>"},
    {U'<', "<lt>"},
}};
constexpr char32_t last_control_letter = 0x1A;
constexpr char32_t delete_code_point = 0x7F;
constexpr char32_t last_code_point = 0x10FFFF;
constexpr char32_t first_surrogate = 0xD800;
constexpr char32_t last_surrogate = 0xDFFF;
constexpr std::string_view unnamed_control = "<C-?>";
constexpr std::string_view lone_less_than = "<";

[[nodiscard]] std::string character_notation(char32_t letter)
{
    const auto named =
        std::ranges::find(character_names, letter, &std::pair<char32_t, std::string_view>::first);
    if (named != character_names.end())
    {
        return std::string(named->second);
    }
    if (letter >= 1 && letter <= last_control_letter)
    {
        return std::string("<C-") + static_cast<char>('a' + (letter - 1)) + ">";
    }
    if (letter < U' ' || letter == delete_code_point || letter > last_code_point ||
        (letter >= first_surrogate && letter <= last_surrogate))
    {
        return std::string(unnamed_control);
    }
    std::string text;
    append_utf8(text, letter);
    return text;
}

// "…" の中の 1 字の逃がし（Vim の expr-quote の `\n` `\t` `\r` `\e` `\b` `\f`）。
constexpr std::array<std::pair<char, char32_t>, 6> letter_escapes{{
    {'n', U'\n'},
    {'t', U'\t'},
    {'r', U'\r'},
    {'e', U'\x1b'},
    {'b', U'\b'},
    {'f', U'\f'},
}};
// 数の逃がし（`\x41` `\X41` は 16 進 2 桁まで・`é` は 4 桁まで・`\U0001F600` は 8 桁まで）。
constexpr std::array<std::pair<char, std::size_t>, 4> hex_escapes{{
    {'x', 2},
    {'X', 2},
    {'u', 4},
    {'U', 8},
}};
constexpr std::size_t octal_digits = 3;
constexpr std::uint32_t octal_base = 8;
constexpr std::uint32_t hex_base = 16;
constexpr std::uint32_t decimal_digits = 10;

// 文字の数値（base の桁でなければ base 以上）。
[[nodiscard]] std::uint32_t digit_value(char letter) noexcept
{
    if (letter >= '0' && letter <= '9')
    {
        return static_cast<std::uint32_t>(letter - '0');
    }
    if (letter >= 'a' && letter <= 'f')
    {
        return static_cast<std::uint32_t>(letter - 'a') + decimal_digits;
    }
    if (letter >= 'A' && letter <= 'F')
    {
        return static_cast<std::uint32_t>(letter - 'A') + decimal_digits;
    }
    return hex_base;
}

// text の先頭から base の桁を limit 桁まで読む（値と読んだ桁の数）。
[[nodiscard]] std::pair<char32_t, std::size_t>
digits_read(std::string_view text, std::uint32_t base, std::size_t limit) noexcept
{
    std::uint32_t value = 0;
    std::size_t read = 0;
    while (read < limit && read < text.size() && digit_value(text.at(read)) < base)
    {
        value = value * base + digit_value(text.at(read));
        ++read;
    }
    return {static_cast<char32_t>(value), read};
}

// 1 字の記法と、逃がしの印 `\` の後ろから読んだバイトの数。
using EscapedUnit = std::pair<std::string, std::size_t>;

// `\<Name>`。Nib の表にある名前はその記法、`<lt>` は `<`、ほかは Vim と同じく `<` の 1 字（後ろの
// 名前は普通の字として続く）。
[[nodiscard]] EscapedUnit named_escape(std::string_view rest)
{
    const auto close = rest.find('>');
    const auto name = rest.substr(0, close == std::string_view::npos ? 0 : close + 1);
    if (name == "<lt>")
    {
        return {character_notation(U'<'), name.size()};
    }
    if (!name.empty() && vim_key_of_name(name).has_value())
    {
        return {std::string(name), name.size()};
    }
    return {character_notation(U'<'), 1};
}

// `\` の後ろ rest（空でない）の逃がしを読む。知らない字はその字のまま（`\q` は `q`）。
[[nodiscard]] EscapedUnit escaped_unit(std::string_view rest)
{
    const char letter = rest.front();
    const auto simple =
        std::ranges::find(letter_escapes, letter, &std::pair<char, char32_t>::first);
    if (simple != letter_escapes.end())
    {
        return {character_notation(simple->second), 1};
    }
    const auto hex = std::ranges::find(hex_escapes, letter, &std::pair<char, std::size_t>::first);
    if (hex != hex_escapes.end())
    {
        const auto [value, read] = digits_read(rest.substr(1), hex_base, hex->second);
        return read == 0 ? EscapedUnit{std::string(1, letter), 1}
                         : EscapedUnit{character_notation(value), read + 1};
    }
    if (letter >= '0' && letter <= '7')
    {
        const auto [value, read] = digits_read(rest, octal_base, octal_digits);
        return {character_notation(value), read};
    }
    if (letter == '<')
    {
        return named_escape(rest);
    }
    const std::size_t length = next_code_point(rest, Offset{0}).value;
    return {character_notation(code_point_at(rest, Offset{0})), length};
}

// 1 字ずつの記法を 1 つの値へ。1 字なら `<lt>` は `<` に戻す（`<` の 1 鍵）。
[[nodiscard]] std::string joined_notation(const std::vector<std::string> &units)
{
    if (units.size() == 1 && units.front() == character_notation(U'<'))
    {
        return std::string(lone_less_than);
    }
    std::string value;
    for (const auto &unit : units)
    {
        value += unit;
    }
    return value;
}

// "…" の中身を Vim の expr-quote として 1 字ずつの記法へ。終わりの `\` は閉じの `"` を逃がすので
// 閉じていない（Vim の E114）。
[[nodiscard]] std::optional<std::vector<std::string>> double_quoted_units(std::string_view inner)
{
    std::vector<std::string> units;
    std::size_t at = 0;
    while (at < inner.size())
    {
        if (inner.at(at) != '\\')
        {
            units.push_back(character_notation(code_point_at(inner, Offset{at})));
            at = next_code_point(inner, Offset{at}).value;
            continue;
        }
        if (at + 1 == inner.size())
        {
            return std::nullopt;
        }
        auto [unit, read] = escaped_unit(inner.substr(at + 1));
        units.push_back(std::move(unit));
        at += 1 + read;
    }
    return units;
}

// '…' の中身（逃がしは `''` の 1 つだけ・`'\n'` は 2 字・`'<Space>'` は 7 字）。
[[nodiscard]] std::vector<std::string> single_quoted_units(std::string_view inner)
{
    std::vector<std::string> units;
    std::size_t at = 0;
    while (at < inner.size())
    {
        units.push_back(character_notation(code_point_at(inner, Offset{at})));
        const bool doubled = inner.substr(at).starts_with("''");
        at = doubled ? at + 2 : next_code_point(inner, Offset{at}).value;
    }
    return units;
}

// Vim の文字列の値を鍵の記法で（`"\<Space>"` と `" "` は `<Space>`・`"\n"` は `<NL>`・#2018）。
// 引用符で囲まれていない・閉じていなければ値なし。
[[nodiscard]] std::optional<std::string> quoted_value(std::string_view text)
{
    if (text.size() < 2 || text.front() != text.back() ||
        (text.front() != '\'' && text.front() != '"') || !validate_utf8(text).has_value())
    {
        return std::nullopt;
    }
    const auto inner = text.substr(1, text.size() - 2);
    if (text.front() == '\'')
    {
        return joined_notation(single_quoted_units(inner));
    }
    const auto units = double_quoted_units(inner);
    if (!units.has_value())
    {
        return std::nullopt;
    }
    return joined_notation(units.value());
}

// `:let mapleader = ","`。ほかの変数は受けない。値は 1 字ずつ鍵の記法に写し、1 鍵かは
// map_edited の parse_map_leader が決める。
[[nodiscard]] Evaluated leader_assigned(std::string_view argument)
{
    if (!argument.starts_with(leader_variable))
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    const auto rest = trimmed(argument.substr(leader_variable.size()));
    if (!rest.starts_with('='))
    {
        return std::unexpected(ExFailure::invalid_argument);
    }
    auto value = quoted_value(trimmed(rest.substr(1)));
    if (!value.has_value())
    {
        return std::unexpected(ExFailure::invalid_argument);
    }
    return map_result(ExMapRequest{ExMapVerb::leader, {}, {}, std::move(value).value(), true},
                      let_name);
}

// `<Leader>` を `<leader>` に揃えた左辺（追補 17・ほかの綴りは表どおりで大小を区別する）。
[[nodiscard]] std::string canonical_lhs(std::string_view text)
{
    std::string value(text);
    for (auto at = value.find(leader_spelling); at != std::string::npos;
         at = value.find(leader_spelling, at + leader_canonical.size()))
    {
        value.replace(at, leader_spelling.size(), leader_canonical);
    }
    return value;
}

[[nodiscard]] bool shares_mode(VimModeSet left, VimModeSet right) noexcept
{
    return (left.normal && right.normal) || (left.insert && right.insert) ||
           ((left.visual || left.visual_only) && (right.visual || right.visual_only)) ||
           (left.operator_pending && right.operator_pending);
}

// modes を外した集合（`v` と `x` は同じ VISUAL なのでどちらかでどちらも外す）。
[[nodiscard]] VimModeSet modes_removed(VimModeSet from, VimModeSet modes) noexcept
{
    const bool visual = modes.visual || modes.visual_only;
    return VimModeSet{from.normal && !modes.normal, from.insert && !modes.insert,
                      from.visual && !visual, from.visual_only && !visual,
                      from.operator_pending && !modes.operator_pending};
}

// rows の左辺 lhs の行から modes を外し、空になった行を消す。外した行があれば真。
[[nodiscard]] bool without_modes(std::vector<MapRow> &rows, VimModeSet modes, std::string_view lhs)
{
    const auto wanted = canonical_lhs(lhs);
    bool touched = false;
    std::vector<MapRow> kept;
    kept.reserve(rows.size());
    for (MapRow &row : rows)
    {
        if (shares_mode(row.modes, modes) && canonical_lhs(row.lhs) == wanted)
        {
            touched = true;
            row.modes = modes_removed(row.modes, modes);
        }
        if (!mode_set_empty(row.modes))
        {
            kept.push_back(std::move(row));
        }
    }
    rows = std::move(kept);
    return touched;
}

// 保存で同じ鍵（map_line_key）になる行は後の行だけを残す（追補 14 の拒否を書き手が作らない）。
[[nodiscard]] std::vector<MapRow> last_of_each_key(const std::vector<MapRow> &rows)
{
    std::vector<MapRow> kept;
    for (auto row = rows.begin(); row != rows.end(); ++row)
    {
        const auto key = map_line_key(*row);
        const bool later = std::any_of(std::next(row), rows.end(), [&key](const MapRow &other)
                                       { return map_line_key(other) == key; });
        if (!later)
        {
            kept.push_back(*row);
        }
    }
    return kept;
}

[[nodiscard]] ExMapFailure ex_map_failure_of(KeyMapFailure failure) noexcept
{
    switch (failure)
    {
    case KeyMapFailure::malformed_key:
    case KeyMapFailure::bad_modes:
    case KeyMapFailure::unknown_key:
        return ExMapFailure::unknown_key;
    case KeyMapFailure::empty_keys:
        return ExMapFailure::empty_keys;
    case KeyMapFailure::long_lhs:
        return ExMapFailure::long_lhs;
    case KeyMapFailure::bad_leader:
        return ExMapFailure::bad_leader;
    case KeyMapFailure::unwritable:
        return ExMapFailure::unwritable;
    }
    std::unreachable();
}

[[nodiscard]] std::expected<EditorSettings, ExMapFailure> with_row(EditorSettings settings,
                                                                   const ExMapRequest &request)
{
    auto row = map_row_of(request.modes, request.lhs, request.rhs, request.remap);
    if (!row.has_value())
    {
        return std::unexpected(ex_map_failure_of(row.error()));
    }
    static_cast<void>(without_modes(settings.maps, request.modes, request.lhs));
    settings.maps.push_back(std::move(row).value());
    settings.maps = last_of_each_key(settings.maps);
    return settings;
}

[[nodiscard]] std::expected<EditorSettings, ExMapFailure> without_row(EditorSettings settings,
                                                                      const ExMapRequest &request)
{
    if (!without_modes(settings.maps, request.modes, request.lhs))
    {
        return std::unexpected(ExMapFailure::no_such_mapping);
    }
    settings.maps = last_of_each_key(settings.maps);
    return settings;
}

[[nodiscard]] std::expected<EditorSettings, ExMapFailure> with_leader(EditorSettings settings,
                                                                      const ExMapRequest &request)
{
    const auto leader = parse_map_leader(request.rhs);
    if (!leader.has_value())
    {
        return std::unexpected(ex_map_failure_of(leader.error()));
    }
    settings.mapleader = request.rhs;
    return settings;
}

[[nodiscard]] Evaluated named_map_command(const ExMapName &row, std::string_view argument)
{
    switch (row.verb)
    {
    case ExMapVerb::add:
    case ExMapVerb::list:
    case ExMapVerb::leader:
        return added(row, argument);
    case ExMapVerb::remove:
        return removed(row, argument);
    }
    std::unreachable();
}
} // namespace

std::optional<std::expected<ExResult, ExEvaluationFailure>> map_command(std::string_view text)
{
    const std::size_t range = leading(text, ascii_digit);
    const std::size_t letters = leading(text.substr(range), ascii_letter);
    const auto name = text.substr(range, letters);
    const auto suffix = text.substr(range + letters);
    const bool plain = range == 0 && (suffix.empty() || suffix.front() == ' ');
    if (name == let_name)
    {
        if (!plain)
        {
            return std::unexpected(ExFailure::unsupported_argument);
        }
        return leader_assigned(trimmed(suffix));
    }
    const auto row = std::ranges::find_if(
        map_names, [name](const ExMapName &entry)
        { return name.size() >= entry.shortest && entry.name.starts_with(name); });
    if (row == map_names.end())
    {
        return std::nullopt;
    }
    if (!plain)
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    return named_map_command(*row, trimmed(suffix));
}

std::span<const std::string_view> map_command_candidates() noexcept
{
    return candidates;
}

std::expected<EditorSettings, ExMapFailure> map_edited(const EditorSettings &settings,
                                                       const ExMapRequest &request)
{
    switch (request.verb)
    {
    case ExMapVerb::add:
        return with_row(settings, request);
    case ExMapVerb::remove:
        return without_row(settings, request);
    case ExMapVerb::leader:
        return with_leader(settings, request);
    case ExMapVerb::list:
        return settings;
    }
    std::unreachable();
}

DisplayText ex_map_failure_message(ExMapFailure failure)
{
    switch (failure)
    {
    case ExMapFailure::unknown_key:
        return DisplayText::parse("Unknown key name in mapping").value();
    case ExMapFailure::empty_keys:
        return DisplayText::parse("E474: Invalid argument").value();
    case ExMapFailure::long_lhs:
        return DisplayText::parse("Mapping keys are limited to 3").value();
    case ExMapFailure::bad_leader:
        return DisplayText::parse("mapleader must be one key").value();
    case ExMapFailure::unwritable:
        return DisplayText::parse("Mapping cannot be saved in settings").value();
    case ExMapFailure::no_such_mapping:
        return DisplayText::parse("E31: No such mapping").value();
    }
    std::unreachable();
}
} // namespace nenenib::core
