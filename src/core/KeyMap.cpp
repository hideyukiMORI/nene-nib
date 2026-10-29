#include "KeyMap.hpp"

#include "Offset.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimKeyNotation.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace nenenib::core
{
namespace
{
using Keys = std::vector<VimKey>;
using KeySpan = std::span<const VimKey>;
using ParsedKeys = std::expected<Keys, KeyMapFailure>;
using ModeFlag = bool VimModeSet::*;

constexpr std::string_view map_prefix = "map.";
constexpr std::array<std::string_view, 2> leader_names{"<leader>", "<Leader>"};
// 設定の 1 行の鍵と値の端で読み手（KeyValueFields）が落とす空白。
constexpr std::string_view trimmed_spaces = " \t\r\v\f";
// 行の検査で `<leader>` の代わりに置く 1 打鍵（打鍵の数だけを数える）。
constexpr VimCharacter counted_leader{U'\\'};
// モードの文字と欄（同じ位置・書き戻しの順）。
constexpr std::string_view mode_letters = "nivxo";
constexpr std::array<ModeFlag, 5> mode_flags{&VimModeSet::normal, &VimModeSet::insert,
                                             &VimModeSet::visual, &VimModeSet::visual_only,
                                             &VimModeSet::operator_pending};

[[nodiscard]] constexpr bool name_character(char letter) noexcept
{
    return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') ||
           (letter >= '0' && letter <= '9') || letter == '-';
}

// 先頭が `<名前>` の形（英数字と `-`）ならその長さ、違えば 0（`<` はただの文字）。
[[nodiscard]] std::size_t name_length(std::string_view text) noexcept
{
    if (!text.starts_with('<'))
    {
        return 0;
    }
    const auto close = text.find('>', 1);
    if (close == std::string_view::npos || close == 1)
    {
        return 0;
    }
    const auto inner = text.substr(1, close - 1);
    return std::ranges::all_of(inner, name_character) ? close + 1 : 0;
}

[[nodiscard]] std::expected<VimKey, KeyMapFailure> named_key(std::string_view name,
                                                             const std::optional<VimKey> &leader)
{
    if (std::ranges::find(leader_names, name) != leader_names.end())
    {
        if (!leader.has_value())
        {
            return std::unexpected(KeyMapFailure::bad_leader);
        }
        return leader.value();
    }
    auto key = vim_key_of_name(name);
    if (!key.has_value())
    {
        return std::unexpected(KeyMapFailure::unknown_key);
    }
    return std::move(key).value();
}

// text の先頭の 1 打鍵を keys に足して、読んだバイト数を返す。
[[nodiscard]] std::expected<std::size_t, KeyMapFailure>
append_key(Keys &keys, std::string_view text, const std::optional<VimKey> &leader)
{
    const auto length = name_length(text);
    if (length == 0)
    {
        keys.emplace_back(VimCharacter{code_point_at(text, Offset{0})});
        return next_code_point(text, Offset{0}).value;
    }
    auto key = named_key(text.substr(0, length), leader);
    if (!key.has_value())
    {
        return std::unexpected(key.error());
    }
    keys.push_back(std::move(key).value());
    return length;
}

// 鍵の記法の文字列を鍵の列へ。leader が無しなら `<leader>` は書けない。
[[nodiscard]] ParsedKeys keys_of_notation(std::string_view text,
                                          const std::optional<VimKey> &leader)
{
    if (!validate_utf8(text).has_value())
    {
        return std::unexpected(KeyMapFailure::unknown_key);
    }
    Keys keys;
    std::size_t at = 0;
    while (at < text.size())
    {
        const auto read = append_key(keys, text.substr(at), leader);
        if (!read.has_value())
        {
            return std::unexpected(read.error());
        }
        at += read.value();
    }
    return keys;
}

[[nodiscard]] std::expected<VimModeSet, KeyMapFailure> modes_of(std::string_view letters)
{
    VimModeSet modes;
    if (letters.empty())
    {
        return std::unexpected(KeyMapFailure::bad_modes);
    }
    for (const char letter : letters)
    {
        const auto index = mode_letters.find(letter);
        if (index == std::string_view::npos || modes.*mode_flags.at(index))
        {
            return std::unexpected(KeyMapFailure::bad_modes);
        }
        modes.*mode_flags.at(index) = true;
    }
    return modes;
}

// 設定の行の鍵か値として書いて同じ文字列に読み戻せるか（改行・端の空白が無い）。
[[nodiscard]] bool line_safe(std::string_view text) noexcept
{
    return text.find('\n') == std::string_view::npos &&
           (text.empty() || (trimmed_spaces.find(text.front()) == std::string_view::npos &&
                             trimmed_spaces.find(text.back()) == std::string_view::npos));
}

[[nodiscard]] std::expected<void, KeyMapFailure> counted_keys(std::string_view lhs,
                                                              std::string_view rhs)
{
    const std::optional<VimKey> leader{VimKey{counted_leader}};
    const auto left = keys_of_notation(lhs, leader);
    const auto right = keys_of_notation(rhs, leader);
    if (!left.has_value())
    {
        return std::unexpected(left.error());
    }
    if (!right.has_value())
    {
        return std::unexpected(right.error());
    }
    if (left.value().empty() || right.value().empty())
    {
        return std::unexpected(KeyMapFailure::empty_keys);
    }
    if (left.value().size() > key_map_lhs_limit)
    {
        return std::unexpected(KeyMapFailure::long_lhs);
    }
    return {};
}

[[nodiscard]] bool applies(const KeyMapping &mapping, VimMapMode mode) noexcept
{
    return mode_set_contains(mapping.modes, mode);
}

// whole が part で始まるか（part が長ければ偽）。
[[nodiscard]] bool starts_with(KeySpan whole, KeySpan part)
{
    return part.size() <= whole.size() && std::ranges::equal(part, whole.first(part.size()));
}

// rest が mode の行の左辺の途中（左辺より短い先頭）か。
[[nodiscard]] bool waits(const KeyMap &map, VimMapMode mode, KeySpan rest)
{
    return std::ranges::any_of(map.mappings,
                               [&](const KeyMapping &mapping)
                               {
                                   return applies(mapping, mode) &&
                                          mapping.lhs.size() > rest.size() &&
                                          starts_with(mapping.lhs, rest);
                               });
}

// rest の先頭に一致する mode の行のうち左辺が最も長いもの。
[[nodiscard]] const KeyMapping *longest(const KeyMap &map, VimMapMode mode, KeySpan rest)
{
    const KeyMapping *found = nullptr;
    for (const auto &mapping : map.mappings)
    {
        if (applies(mapping, mode) && starts_with(rest, mapping.lhs) &&
            (found == nullptr || mapping.lhs.size() > found->lhs.size()))
        {
            found = &mapping;
        }
    }
    return found;
}

void append(Keys &out, KeySpan keys)
{
    out.insert(out.end(), keys.begin(), keys.end());
}

// 右辺の中を 1 段だけ引く（待たない・見つけた行の右辺はそのまま）。
void append_once(Keys &out, const KeyMap &map, VimMapMode mode, KeySpan keys)
{
    std::size_t at = 0;
    while (at < keys.size())
    {
        const auto *found = longest(map, mode, keys.subspan(at));
        if (found == nullptr)
        {
            out.push_back(keys[at]);
            ++at;
            continue;
        }
        append(out, found->rhs);
        at += found->lhs.size();
    }
}

void append_mapping(Keys &out, const KeyMap &map, VimMapMode mode, const KeyMapping &mapping)
{
    KeySpan rhs{mapping.rhs};
    if (!mapping.remap)
    {
        append(out, rhs);
        return;
    }
    if (starts_with(rhs, mapping.lhs))
    {
        append(out, mapping.lhs);
        rhs = rhs.subspan(mapping.lhs.size());
    }
    append_once(out, map, mode, rhs);
}

// rest の先頭の 1 つ（左辺が最も長い行か、無ければ 1 鍵）を keys に流す。読んだ鍵の数と、
// 表の行に当たったかを返す。
[[nodiscard]] std::pair<std::size_t, bool> consume(const KeyMap &map, VimMapMode mode, KeySpan rest,
                                                   Keys &keys)
{
    const auto *found = longest(map, mode, rest);
    if (found == nullptr)
    {
        keys.push_back(rest.front());
        return {1U, false};
    }
    append_mapping(keys, map, mode, *found);
    return {found->lhs.size(), true};
}

// 後の行が前の行の同じ左辺のモードを上書きする（`v` と `x` は同じ VISUAL）。
void override_modes(KeyMap &map, const KeyMapping &later)
{
    const bool visual = later.modes.visual || later.modes.visual_only;
    for (auto &earlier : map.mappings)
    {
        if (earlier.lhs != later.lhs)
        {
            continue;
        }
        earlier.modes.normal = earlier.modes.normal && !later.modes.normal;
        earlier.modes.insert = earlier.modes.insert && !later.modes.insert;
        earlier.modes.visual = earlier.modes.visual && !visual;
        earlier.modes.visual_only = earlier.modes.visual_only && !visual;
        earlier.modes.operator_pending =
            earlier.modes.operator_pending && !later.modes.operator_pending;
    }
    std::erase_if(map.mappings,
                  [](const KeyMapping &mapping) { return mode_set_empty(mapping.modes); });
}
} // namespace

std::expected<MapRow, KeyMapFailure> map_row_of(VimModeSet modes, std::string_view lhs,
                                                std::string_view rhs, bool remap)
{
    if (mode_set_empty(modes))
    {
        return std::unexpected(KeyMapFailure::bad_modes);
    }
    if (!line_safe(lhs) || !line_safe(rhs) || lhs.find('=') != std::string_view::npos ||
        (remap && rhs.starts_with('!')))
    {
        return std::unexpected(KeyMapFailure::unwritable);
    }
    const auto counted = counted_keys(lhs, rhs);
    if (!counted.has_value())
    {
        return std::unexpected(counted.error());
    }
    return MapRow{modes, std::string(lhs), std::string(rhs), remap};
}

std::expected<MapRow, KeyMapFailure> parse_map_line(std::string_view key, std::string_view value)
{
    if (!key.starts_with(map_prefix))
    {
        return std::unexpected(KeyMapFailure::malformed_key);
    }
    const auto rest = key.substr(map_prefix.size());
    const auto dot = rest.find('.');
    if (dot == std::string_view::npos)
    {
        return std::unexpected(KeyMapFailure::malformed_key);
    }
    const auto modes = modes_of(rest.substr(0, dot));
    if (!modes.has_value())
    {
        return std::unexpected(modes.error());
    }
    const bool remap = !value.starts_with('!');
    return map_row_of(modes.value(), rest.substr(dot + 1), remap ? value : value.substr(1), remap);
}

std::string map_line_key(const MapRow &row)
{
    std::string key(map_prefix);
    for (std::size_t index = 0; index < mode_letters.size(); ++index)
    {
        if (row.modes.*mode_flags.at(index))
        {
            key += mode_letters[index];
        }
    }
    key += '.';
    key += row.lhs;
    return key;
}

std::string map_line_value(const MapRow &row)
{
    return row.remap ? row.rhs : "!" + row.rhs;
}

std::expected<VimKey, KeyMapFailure> parse_map_leader(std::string_view text)
{
    // 端の空白は設定の行の読み手が落とすので書けない（空白の leader は `<Space>` と書く）。
    auto keys = keys_of_notation(text, std::nullopt);
    if (!line_safe(text) || !keys.has_value() || keys.value().size() != 1)
    {
        return std::unexpected(KeyMapFailure::bad_leader);
    }
    return std::move(keys).value().front();
}

std::expected<KeyMap, KeyMapFailure> key_map_of(const EditorSettings &settings)
{
    const auto leader = parse_map_leader(settings.mapleader);
    if (!leader.has_value())
    {
        return std::unexpected(leader.error());
    }
    KeyMap map;
    for (const auto &row : settings.maps)
    {
        const auto checked = map_row_of(row.modes, row.lhs, row.rhs, row.remap);
        if (!checked.has_value())
        {
            return std::unexpected(checked.error());
        }
        auto lhs = keys_of_notation(row.lhs, leader.value());
        auto rhs = keys_of_notation(row.rhs, leader.value());
        KeyMapping mapping{row.modes, std::move(lhs).value(), std::move(rhs).value(), row.remap};
        override_modes(map, mapping);
        map.mappings.push_back(std::move(mapping));
    }
    return map;
}

std::optional<VimMapMode> vim_map_mode_of(const VimState &state) noexcept
{
    if (state.input_wait.has_value())
    {
        return std::nullopt;
    }
    switch (state.mode)
    {
    case VimMode::normal:
        return state.pending.has_value() ? VimMapMode::operator_pending : VimMapMode::normal;
    case VimMode::insert:
        return VimMapMode::insert;
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        return VimMapMode::visual;
    }
    std::unreachable();
}

KeyMapStep key_map_step(const KeyMap &map, VimMapMode mode, std::span<const VimKey> pending,
                        const VimKey &key)
{
    if (map.mappings.empty())
    {
        return KeyMapPass{};
    }
    Keys buffer(pending.begin(), pending.end());
    buffer.push_back(key);
    const KeySpan whole{buffer};
    Keys keys;
    bool mapped = false;
    std::size_t at = 0;
    while (at < whole.size() && !waits(map, mode, whole.subspan(at)))
    {
        const auto [count, row] = consume(map, mode, whole.subspan(at), keys);
        at += count;
        mapped = mapped || row;
    }
    if (keys.empty())
    {
        return KeyMapPending{};
    }
    if (!mapped && pending.empty())
    {
        return KeyMapPass{};
    }
    return KeyMapEmit{std::move(keys),
                      Keys(whole.begin() + static_cast<std::ptrdiff_t>(at), whole.end())};
}

std::vector<VimKey> key_map_flush(const KeyMap &map, VimMapMode mode,
                                  std::span<const VimKey> pending)
{
    Keys keys;
    std::size_t at = 0;
    while (at < pending.size())
    {
        at += consume(map, mode, pending.subspan(at), keys).first;
    }
    return keys;
}
} // namespace nenenib::core
