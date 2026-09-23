#include "VimRegisterText.hpp"

#include "Offset.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchPattern.hpp"
#include "VimSpecialKey.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::core
{
namespace
{
// 特殊鍵 → Vim が typeahead に置くバイト（ADR 0048 の決定 7）。鍵が増えたらここでコンパイルが
// 落ちる（CPP-002）。`<80>` は U+0080 の UTF-8（`\xC2\x80`）で、Vim の K_SPECIAL と同じ見た目。
[[nodiscard]] constexpr std::string_view special_key_text(VimSpecialKey key) noexcept
{
    switch (key)
    {
    case VimSpecialKey::escape:
        return "\x1b";
    case VimSpecialKey::enter:
        return "\r";
    case VimSpecialKey::backspace:
        return "\xC2\x80kb";
    case VimSpecialKey::arrow_left:
        return "\xC2\x80kl";
    case VimSpecialKey::arrow_right:
        return "\xC2\x80kr";
    case VimSpecialKey::arrow_up:
        return "\xC2\x80ku";
    case VimSpecialKey::arrow_down:
        return "\xC2\x80kd";
    case VimSpecialKey::control_r:
        return "\x12";
    case VimSpecialKey::home:
        return "\xC2\x80kh";
    case VimSpecialKey::end:
        return "\xC2\x80@7";
    case VimSpecialKey::page_up:
        return "\xC2\x80kP";
    case VimSpecialKey::page_down:
        return "\xC2\x80kN";
    case VimSpecialKey::control_d:
        return "\x04";
    case VimSpecialKey::control_u:
        return "\x15";
    case VimSpecialKey::control_f:
        return "\x06";
    case VimSpecialKey::control_b:
        return "\x02";
    case VimSpecialKey::control_v:
        return "\x16";
    }
    std::unreachable();
}

// 本文 → 鍵で照らす特殊鍵の一覧。バイト列は special_key_text の 1 か所にだけ書き、ここは
// 列挙子を並べるだけ（鍵を足したら switch が落ちるので、ここにも 1 行足す）。
constexpr std::array special_keys{
    VimSpecialKey::escape,     VimSpecialKey::enter,       VimSpecialKey::backspace,
    VimSpecialKey::arrow_left, VimSpecialKey::arrow_right, VimSpecialKey::arrow_up,
    VimSpecialKey::arrow_down, VimSpecialKey::control_r,   VimSpecialKey::home,
    VimSpecialKey::end,        VimSpecialKey::page_up,     VimSpecialKey::page_down,
    VimSpecialKey::control_d,  VimSpecialKey::control_u,   VimSpecialKey::control_f,
    VimSpecialKey::control_b,  VimSpecialKey::control_v};

// Vim の Ctrl-H。本文 → 鍵でだけ backspace に読む（鍵 → 本文は `<80>kb`）。
constexpr std::string_view control_h = "\x08";

[[nodiscard]] char search_prefix(VimSearchDirection direction) noexcept
{
    switch (direction)
    {
    case VimSearchDirection::forward:
        return '/';
    case VimSearchDirection::backward:
        return '?';
    }
    std::unreachable();
}

void append_key_text(std::string &text, const VimKey &key)
{
    std::visit(
        [&text](const auto &value)
        {
            using Key = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Key, VimCharacter>)
            {
                append_utf8(text, value.code);
            }
            else if constexpr (std::is_same_v<Key, VimSpecialKey>)
            {
                text.append(special_key_text(value));
            }
            else
            {
                static_assert(std::is_same_v<Key, VimSearchPattern>);
                text.push_back(search_prefix(value.direction));
                text.append(value.pattern);
                text.push_back('\r');
            }
        },
        key);
}

// 本文の先頭から読める特殊鍵と、その本文のバイト数。
using SpecialKeyText = std::pair<VimSpecialKey, std::size_t>;

// rest の先頭の特殊鍵。無ければ位置を持たない（先頭は文字として読む）。
[[nodiscard]] std::optional<SpecialKeyText> special_key_at(std::string_view rest) noexcept
{
    if (rest.starts_with(control_h))
    {
        return SpecialKeyText{VimSpecialKey::backspace, control_h.size()};
    }
    for (const VimSpecialKey key : special_keys)
    {
        const std::string_view encoded = special_key_text(key);
        if (rest.starts_with(encoded))
        {
            return SpecialKeyText{key, encoded.size()};
        }
    }
    return std::nullopt;
}
} // namespace

std::string vim_register_text(std::span<const VimKey> keys)
{
    std::string text;
    for (const VimKey &key : keys)
    {
        append_key_text(text, key);
    }
    return text;
}

std::vector<VimKey> vim_keys_of_text(std::string_view text)
{
    std::vector<VimKey> keys;
    std::size_t at = 0;
    while (at < text.size())
    {
        const auto special = special_key_at(text.substr(at));
        if (special.has_value())
        {
            keys.emplace_back(special.value().first);
            at += special.value().second;
            continue;
        }
        keys.emplace_back(VimCharacter{code_point_at(text, Offset{at})});
        at = next_code_point(text, Offset{at}).value;
    }
    return keys;
}
} // namespace nenenib::core
