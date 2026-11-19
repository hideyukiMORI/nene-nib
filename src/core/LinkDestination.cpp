#include "LinkDestination.hpp"

#include "AsciiCase.hpp"
#include "FilePath.hpp"
#include "LinkKind.hpp"
#include "LocalReference.hpp"
#include "Offset.hpp"
#include "PercentDecoding.hpp"
#include "UnlistedExtensions.hpp"
#include "UriScheme.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
constexpr std::string_view hex_digits = "0123456789ABCDEF";
// RFC 3986 の unreserved と reserved の印（英数字と `%` は別に見る）。
// ほかの ASCII（空白・`"` `<` `>` `\` `^` `` ` `` `{` `|` `}`）を含む URI は断る。
constexpr std::string_view uri_marks = "-._~:/?#[]@!$&'()*+,;=";
// mailto で受ける引数の名前（attach などファイルを添える引数は断る）。
constexpr auto mail_fields = std::to_array<std::string_view>({"subject", "body", "cc", "bcc"});
// 装置の予約名（最初の `.` より前・ASCII の大小は畳む）。
// COM と LPT は後ろの 1 字を別に見る。
constexpr auto device_names =
    std::to_array<std::string_view>({"CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$"});
constexpr auto numbered_devices = std::to_array<std::string_view>({"COM", "LPT"});
// COM と LPT の後ろに付く上付きの ¹ ² ³（UTF-8）。
constexpr auto superscript_digits =
    std::to_array<std::string_view>({"\xC2\xB9", "\xC2\xB2", "\xC2\xB3"});

[[nodiscard]] bool ascii_alphanumeric(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9');
}

[[nodiscard]] bool hex_digit(char value) noexcept
{
    return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

[[nodiscard]] bool non_ascii(char value) noexcept
{
    return static_cast<unsigned char>(value) >= 0x80U;
}

[[nodiscard]] LinkDestination spelled(LinkKind kind, std::string_view text)
{
    return LinkDestination{.kind = kind, .text = std::string(text)};
}

// 1 バイトずつ歩く検索（STL の検索は Debug で memchr を core の外へ出す・ARC-003）。
[[nodiscard]] bool one_of(char value, std::string_view set) noexcept
{
    return std::ranges::any_of(set, [value](char listed) { return listed == value; });
}

// text の中の set のどれかの最初の位置（無ければ npos）。
[[nodiscard]] std::size_t first_of(std::string_view text, std::string_view set) noexcept
{
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (one_of(text[index], set))
        {
            return index;
        }
    }
    return std::string_view::npos;
}

// text の中の value の最後の位置（無ければ npos）。
[[nodiscard]] std::size_t last_of(std::string_view text, char value) noexcept
{
    for (std::size_t index = text.size(); index > 0U; --index)
    {
        if (text[index - 1U] == value)
        {
            return index - 1U;
        }
    }
    return std::string_view::npos;
}

// at の `%` の後ろに 16 進 2 桁が続くか。
[[nodiscard]] bool complete_percent(std::string_view text, std::size_t at) noexcept
{
    return at + 2U < text.size() && hex_digit(text[at + 1U]) && hex_digit(text[at + 2U]);
}

// web と mail の綴りを OS に渡せる ASCII の URI にする。非 ASCII のバイトは `%XX` に
// 符号化する。RFC 3986 に無い ASCII と不完全な `%` があれば無し。
[[nodiscard]] std::optional<std::string> ascii_uri(std::string_view target)
{
    std::string uri;
    for (std::size_t index = 0; index < target.size(); ++index)
    {
        const char value = target[index];
        if (non_ascii(value))
        {
            const auto byte = static_cast<unsigned char>(value);
            uri.push_back('%');
            uri.push_back(hex_digits[byte >> 4U]);
            uri.push_back(hex_digits[byte & 0x0FU]);
            continue;
        }
        const bool percent = value == '%' && complete_percent(target, index);
        if (!percent && !ascii_alphanumeric(value) && !one_of(value, uri_marks))
        {
            return std::nullopt;
        }
        uri.push_back(value);
    }
    return uri;
}

// http / https の `:` の後ろ rest が `//` と空でない ASCII の host で始まるか。
[[nodiscard]] bool has_web_host(std::string_view rest) noexcept
{
    if (!rest.starts_with("//"))
    {
        return false;
    }
    std::string_view authority = rest.substr(2U);
    authority = authority.substr(0U, first_of(authority, "/?#"));
    if (std::ranges::any_of(authority, non_ascii))
    {
        return false;
    }
    const std::size_t at = last_of(authority, '@');
    std::string_view host = at == std::string_view::npos ? authority : authority.substr(at + 1U);
    if (!host.starts_with('['))
    {
        host = host.substr(0U, last_of(host, ':'));
    }
    return !host.empty();
}

// mailto の `?` の後ろの引数が subject / body / cc / bcc だけか。
// 名前は `%` を解いてから比べる。
[[nodiscard]] bool known_mail_fields(std::string_view target)
{
    const std::size_t question = first_of(target, "?");
    std::string_view rest =
        question == std::string_view::npos ? std::string_view{} : target.substr(question + 1U);
    while (!rest.empty())
    {
        const std::size_t end = first_of(rest, "&");
        const std::string_view field = rest.substr(0U, end);
        const std::string name = percent_decoded(field.substr(0U, first_of(field, "=")));
        const auto named = [&name](std::string_view listed)
        { return ascii_equal_ignoring_case(name, listed); };
        if (!field.empty() && !std::ranges::any_of(mail_fields, named))
        {
            return false;
        }
        rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1U);
    }
    return true;
}

[[nodiscard]] LinkDestination uri_of(LinkKind kind, std::string_view target)
{
    const auto uri = ascii_uri(target);
    if (!uri.has_value())
    {
        return spelled(LinkKind::refused, target);
    }
    return LinkDestination{.kind = kind, .text = uri.value()};
}

// 確かめで見せる綴りを偽れる字（C1 制御・幅 0 の字・双方向の制御・行と段落の区切り）。
[[nodiscard]] bool deceptive_code_point(char32_t value) noexcept
{
    return (value >= 0x80U && value <= 0x9FU) || (value >= 0x200BU && value <= 0x200FU) ||
           (value >= 0x2028U && value <= 0x202EU) || (value >= 0x2060U && value <= 0x2069U) ||
           value == 0xFEFFU;
}

// 検証済みの text が deceptive_code_point の字を含むか。
[[nodiscard]] bool has_deceptive_code_point(std::string_view text) noexcept
{
    for (Offset at{0U}; at.value < text.size(); at = next_code_point(text, at))
    {
        if (deceptive_code_point(code_point_at(text, at)))
        {
            return true;
        }
    }
    return false;
}

// 経路の最後の段 name の、最初の `.` より前が装置の予約名か。
[[nodiscard]] bool device_name(std::string_view name) noexcept
{
    const std::string_view stem = name.substr(0U, first_of(name, "."));
    const auto named = [&stem](std::string_view listed)
    { return ascii_equal_ignoring_case(stem, listed); };
    if (std::ranges::any_of(device_names, named))
    {
        return true;
    }
    for (const std::string_view prefix : numbered_devices)
    {
        if (stem.size() <= prefix.size() || !ascii_prefix_ignoring_case(stem, prefix))
        {
            continue;
        }
        const std::string_view suffix = stem.substr(prefix.size());
        const bool digit = suffix.size() == 1U && suffix[0] >= '0' && suffix[0] <= '9';
        if (digit || std::ranges::contains(superscript_digits, suffix))
        {
            return true;
        }
    }
    return false;
}

// 開かない最後の段（装置の予約名・`.` か空白で終わる名前・テキストでない拡張子）。
[[nodiscard]] bool refused_name(std::string_view name) noexcept
{
    return name.empty() || name.back() == '.' || name.back() == ' ' || device_name(name) ||
           !folder_lists(name);
}

// ローカルの参照（scheme の無い宛先と file: の URL）。解き方は画像と同じ 1 本。
[[nodiscard]] LinkDestination local(std::string_view target,
                                    const std::optional<FilePath> &document)
{
    const auto path = resolve_local_reference(target, document);
    if (!path || has_deceptive_code_point(path.value().text()) ||
        refused_name(path.value().file_name()))
    {
        return spelled(LinkKind::refused, target);
    }
    return spelled(LinkKind::document, path.value().text());
}

// scheme のある宛先。length は `:` の前までの長さ。
[[nodiscard]] LinkDestination schemed(std::string_view target, std::size_t length,
                                      const std::optional<FilePath> &document)
{
    const std::string_view scheme = target.substr(0U, length);
    if (ascii_equal_ignoring_case(scheme, "http") || ascii_equal_ignoring_case(scheme, "https"))
    {
        return has_web_host(target.substr(length + 1U)) ? uri_of(LinkKind::web, target)
                                                        : spelled(LinkKind::refused, target);
    }
    if (ascii_equal_ignoring_case(scheme, "mailto"))
    {
        return known_mail_fields(target) ? uri_of(LinkKind::mail, target)
                                         : spelled(LinkKind::refused, target);
    }
    if (ascii_equal_ignoring_case(scheme, "file"))
    {
        return local(target, document);
    }
    return spelled(LinkKind::refused, target);
}
} // namespace

LinkDestination link_destination(std::string_view target, const std::optional<FilePath> &document)
{
    if (target.empty() || target.front() == ' ' || target.back() == ' ' ||
        has_control_character(target) || !validate_utf8(target))
    {
        return spelled(LinkKind::refused, target);
    }
    if (target.front() == '#')
    {
        return spelled(LinkKind::heading, target.substr(1));
    }
    const auto length = uri_scheme_length(target);
    return length.has_value() ? schemed(target, length.value(), document) : local(target, document);
}
} // namespace nenenib::core
