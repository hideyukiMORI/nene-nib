#include "ImageReference.hpp"

#include "FilePath.hpp"
#include "ImageReferenceRefusal.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
constexpr char native_separator = '\\';
constexpr std::string_view localhost = "localhost";
constexpr std::size_t drive_length = 2;
constexpr unsigned hex_radix = 16U;
constexpr unsigned decimal_digits = 10U;

[[nodiscard]] bool separator(char value) noexcept
{
    return value == '/' || value == '\\';
}

[[nodiscard]] bool ascii_letter(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

[[nodiscard]] bool ascii_digit(char value) noexcept
{
    return value >= '0' && value <= '9';
}

[[nodiscard]] char ascii_lower(char value) noexcept
{
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

[[nodiscard]] bool same_ignoring_case(std::string_view left, std::string_view right) noexcept
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (ascii_lower(left[index]) != ascii_lower(right[index]))
        {
            return false;
        }
    }
    return true;
}

// 16 進の 1 桁の値（16 進でなければ nullopt）。
[[nodiscard]] std::optional<unsigned> hex_digit(char value) noexcept
{
    if (ascii_digit(value))
    {
        return static_cast<unsigned>(value - '0');
    }
    const char lower = ascii_lower(value);
    if (lower >= 'a' && lower <= 'f')
    {
        return static_cast<unsigned>(lower - 'a') + decimal_digits;
    }
    return std::nullopt;
}

// `C:\` か `C:/` で始まる。
[[nodiscard]] bool drive_absolute(std::string_view text) noexcept
{
    return text.size() > drive_length && ascii_letter(text[0]) && text[1] == ':' &&
           separator(text[2]);
}

// `\\` `//` `\/` `/\` で始まる（UNC・host のある参照）。
[[nodiscard]] bool network_prefix(std::string_view text) noexcept
{
    return text.size() >= 2U && separator(text[0]) && separator(text[1]);
}

// scheme の 2 文字目から後ろに来てよい文字（英数字と `+` `-` `.`）。
[[nodiscard]] bool scheme_character(char value) noexcept
{
    return ascii_letter(value) || ascii_digit(value) || value == '+' || value == '-' ||
           value == '.';
}

// scheme（英字で始まり英数字と `+` `-` `.` が続いて `:` で終わる・2 文字以上）の長さ。1 文字はドラ
// イブの文字なので scheme ではない。
[[nodiscard]] std::optional<std::size_t> scheme_length(std::string_view text) noexcept
{
    if (text.empty() || !ascii_letter(text[0]))
    {
        return std::nullopt;
    }
    std::size_t index = 1;
    while (index < text.size() && scheme_character(text[index]))
    {
        ++index;
    }
    if (index < drive_length || index == text.size() || text[index] != ':')
    {
        return std::nullopt;
    }
    return index;
}

// `?` か `#` より前（URL の query と fragment を捨てる）。
[[nodiscard]] std::string_view without_suffix(std::string_view text) noexcept
{
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (text[index] == '?' || text[index] == '#')
        {
            return text.substr(0, index);
        }
    }
    return text;
}

// `%XX` を 1 バイトに解く。16 進 2 桁が続かない `%` はそのまま残す。
[[nodiscard]] std::string percent_decoded(std::string_view text)
{
    std::string decoded;
    decoded.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const auto high = text[index] == '%' && index + 2U < text.size()
                              ? hex_digit(text[index + 1U])
                              : std::nullopt;
        const auto low = high.has_value() ? hex_digit(text[index + 2U]) : std::nullopt;
        if (low.has_value())
        {
            decoded.push_back(static_cast<char>((high.value() * hex_radix) + low.value()));
            index += 2U;
            continue;
        }
        decoded.push_back(text[index]);
    }
    return decoded;
}

// 段を 1 つ積む。`.` と空の段は落とし、`..` は 1 つ上へ畳む（根より上へは出ない）。
void push_part(std::vector<std::string_view> &parts, std::string_view part)
{
    if (part == "..")
    {
        if (!parts.empty())
        {
            parts.pop_back();
        }
        return;
    }
    if (!part.empty() && part != ".")
    {
        parts.push_back(part);
    }
}

// ドライブの絶対パスを `\` で区切り直し、`.` と空の段を落として `..` を 1 つ上へ畳む（根より上へは
// 出ない）。
[[nodiscard]] std::string normalized(std::string_view path)
{
    std::vector<std::string_view> parts;
    std::size_t start = drive_length + 1U;
    for (std::size_t index = start; index <= path.size(); ++index)
    {
        if (index == path.size() || separator(path[index]))
        {
            push_part(parts, path.substr(start, index - start));
            start = index + 1U;
        }
    }
    std::string result(path.substr(0, drive_length));
    result.push_back(native_separator);
    for (std::size_t index = 0; index < parts.size(); ++index)
    {
        if (index != 0U)
        {
            result.push_back(native_separator);
        }
        result.append(parts[index]);
    }
    return result;
}

// ドライブの絶対パスを畳んで経路にする。UTF-8 でない・制御文字を含むなら malformed。
[[nodiscard]] std::expected<FilePath, ImageReferenceRefusal> local_path(std::string_view path)
{
    auto parsed = FilePath::parse(normalized(path));
    if (!parsed)
    {
        return std::unexpected(ImageReferenceRefusal::malformed);
    }
    return std::move(parsed).value();
}

// `file:` の後ろの `//host` を外した経路。host は空か localhost だけを受け、ほかは network。
[[nodiscard]] std::expected<std::string_view, ImageReferenceRefusal>
without_host(std::string_view path) noexcept
{
    if (path.size() < 2U || path[0] != '/' || path[1] != '/')
    {
        return path;
    }
    std::size_t host_end = 2U;
    while (host_end < path.size() && !separator(path[host_end]))
    {
        ++host_end;
    }
    const std::string_view host = path.substr(2U, host_end - 2U);
    if (!host.empty() && !same_ignoring_case(host, localhost))
    {
        return std::unexpected(ImageReferenceRefusal::network);
    }
    return path.substr(host_end);
}

// `file:` の後ろ。`//` の後ろの host は空か localhost だけを受け、`/C:/` の頭の `/` を落とす。
[[nodiscard]] std::expected<FilePath, ImageReferenceRefusal> file_url(std::string_view rest)
{
    const auto path = without_host(without_suffix(rest));
    if (!path)
    {
        return std::unexpected(path.error());
    }
    std::string decoded = percent_decoded(path.value());
    std::string_view local = decoded;
    if (!local.empty() && separator(local[0]) && drive_absolute(local.substr(1)))
    {
        local = local.substr(1);
    }
    if (!drive_absolute(local))
    {
        return std::unexpected(ImageReferenceRefusal::malformed);
    }
    return local_path(local);
}

// 相対参照を文書のフォルダ（`\` か `/` で始まるならドライブの根）から解く。
[[nodiscard]] std::expected<FilePath, ImageReferenceRefusal>
relative_path(std::string_view reference, const std::optional<FilePath> &document)
{
    const std::string decoded = percent_decoded(without_suffix(reference));
    if (decoded.empty())
    {
        return std::unexpected(ImageReferenceRefusal::empty);
    }
    if (!document.has_value())
    {
        return std::unexpected(ImageReferenceRefusal::no_base);
    }
    const std::string_view base = document.value().text();
    if (network_prefix(base))
    {
        return std::unexpected(ImageReferenceRefusal::network);
    }
    if (!drive_absolute(base))
    {
        return std::unexpected(ImageReferenceRefusal::no_base);
    }
    const std::string_view name = document.value().file_name();
    std::string joined(separator(decoded[0]) ? base.substr(0, drive_length)
                                             : base.substr(0, base.size() - name.size()));
    if (!separator(decoded[0]) && !separator(joined.back()))
    {
        joined.push_back(native_separator);
    }
    joined.append(decoded);
    return local_path(joined);
}

// scheme のある参照。
[[nodiscard]] std::expected<FilePath, ImageReferenceRefusal> schemed(std::string_view reference,
                                                                     std::size_t length)
{
    const std::string_view scheme = reference.substr(0, length);
    if (same_ignoring_case(scheme, "http") || same_ignoring_case(scheme, "https"))
    {
        return std::unexpected(ImageReferenceRefusal::remote);
    }
    if (same_ignoring_case(scheme, "data"))
    {
        return std::unexpected(ImageReferenceRefusal::data);
    }
    if (same_ignoring_case(scheme, "file"))
    {
        return file_url(reference.substr(length + 1U));
    }
    return std::unexpected(ImageReferenceRefusal::unknown_scheme);
}
} // namespace

std::expected<FilePath, ImageReferenceRefusal>
resolve_image_reference(std::string_view reference, const std::optional<FilePath> &document)
{
    if (reference.empty())
    {
        return std::unexpected(ImageReferenceRefusal::empty);
    }
    if (drive_absolute(reference))
    {
        return local_path(reference);
    }
    if (network_prefix(reference))
    {
        return std::unexpected(ImageReferenceRefusal::network);
    }
    if (const auto length = scheme_length(reference); length.has_value())
    {
        return schemed(reference, length.value());
    }
    if (reference.size() >= drive_length && ascii_letter(reference[0]) && reference[1] == ':')
    {
        return std::unexpected(ImageReferenceRefusal::malformed);
    }
    return relative_path(reference, document);
}
} // namespace nenenib::core
