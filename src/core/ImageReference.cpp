#include "ImageReference.hpp"

#include "AsciiCase.hpp"
#include "FilePath.hpp"
#include "FileUrl.hpp"
#include "FileUrlReading.hpp"
#include "FileUrlRefusal.hpp"
#include "ImageReferenceRefusal.hpp"
#include "PercentDecoding.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr char native_separator = '\\';
constexpr std::size_t drive_length = 2;

[[nodiscard]] bool ascii_letter(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

[[nodiscard]] bool ascii_digit(char value) noexcept
{
    return value >= '0' && value <= '9';
}

[[nodiscard]] bool same_ignoring_case(std::string_view left, std::string_view right) noexcept
{
    return left.size() == right.size() && ascii_prefix_ignoring_case(left, right);
}

// `\\` `//` `\/` `/\` で始まる（UNC・host のある参照）。
[[nodiscard]] bool network_prefix(std::string_view text) noexcept
{
    return text.size() >= 2U && path_separator(text[0]) && path_separator(text[1]);
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

// 経路にする。UTF-8 でない・制御文字を含むなら malformed。
[[nodiscard]] std::expected<FilePath, ImageReferenceRefusal> parsed_path(std::string_view path)
{
    auto parsed = FilePath::parse(path);
    if (!parsed)
    {
        return std::unexpected(ImageReferenceRefusal::malformed);
    }
    return std::move(parsed).value();
}

// ドライブの絶対パスを `\` で区切り直し、`.` と空の段を落として `..` を 1
// つ上へ畳んで（根より上へは 出ない）経路にする。畳みは core の collapsed_path の 1 本。
[[nodiscard]] std::expected<FilePath, ImageReferenceRefusal> local_path(std::string_view path)
{
    return parsed_path(
        collapsed_path(path.substr(0, drive_length), path.substr(drive_length + 1U)).text);
}

// `file:` の後ろ。写しは core の file_url_path の 1 本（host は空か localhost だけ・ほかは
// network）。
[[nodiscard]] std::expected<FilePath, ImageReferenceRefusal> file_url(std::string_view rest)
{
    const auto path = file_url_path(rest, FileUrlReading::image_reference);
    if (path)
    {
        return parsed_path(path.value());
    }
    switch (path.error())
    {
    case FileUrlRefusal::network:
    case FileUrlRefusal::device:
        return std::unexpected(ImageReferenceRefusal::network);
    case FileUrlRefusal::malformed:
    case FileUrlRefusal::above_root:
        return std::unexpected(ImageReferenceRefusal::malformed);
    }
    return std::unexpected(ImageReferenceRefusal::malformed);
}

// 相対参照を文書のフォルダ（`\` か `/` で始まるならドライブの根）から解く。
[[nodiscard]] std::expected<FilePath, ImageReferenceRefusal>
relative_path(std::string_view reference, const std::optional<FilePath> &document)
{
    const std::string decoded = percent_decoded(url_without_suffix(reference));
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
    if (!drive_absolute_path(base))
    {
        return std::unexpected(ImageReferenceRefusal::no_base);
    }
    const std::string_view name = document.value().file_name();
    std::string joined(path_separator(decoded[0]) ? base.substr(0, drive_length)
                                                  : base.substr(0, base.size() - name.size()));
    if (!path_separator(decoded[0]) && !path_separator(joined.back()))
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
    if (drive_absolute_path(reference))
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
