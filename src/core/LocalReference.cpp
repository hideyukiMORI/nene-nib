#include "LocalReference.hpp"

#include "AsciiCase.hpp"
#include "FilePath.hpp"
#include "FileUrl.hpp"
#include "FileUrlReading.hpp"
#include "FileUrlRefusal.hpp"
#include "LocalReferenceRefusal.hpp"
#include "PercentDecoding.hpp"
#include "UriScheme.hpp"

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

// `\\` `//` `\/` `/\` で始まる（UNC・host のある参照）。
[[nodiscard]] bool network_prefix(std::string_view text) noexcept
{
    return text.size() >= 2U && path_separator(text[0]) && path_separator(text[1]);
}

// 経路にする。UTF-8 でない・制御文字を含むなら malformed。
[[nodiscard]] std::expected<FilePath, LocalReferenceRefusal> parsed_path(std::string_view path)
{
    auto parsed = FilePath::parse(path);
    if (!parsed)
    {
        return std::unexpected(LocalReferenceRefusal::malformed);
    }
    return std::move(parsed).value();
}

// ドライブの絶対パスを `\` で区切り直し、`.` と空の段を落として `..` を 1
// つ上へ畳んで（根より上へは 出ない）経路にする。畳みは core の collapsed_path の 1 本。
[[nodiscard]] std::expected<FilePath, LocalReferenceRefusal> local_path(std::string_view path)
{
    return parsed_path(
        collapsed_path(path.substr(0, drive_length), path.substr(drive_length + 1U)).text);
}

// `file:` の後ろ。写しは core の file_url_path の 1 本（host は空か localhost だけ・ほかは
// network）。
[[nodiscard]] std::expected<FilePath, LocalReferenceRefusal> file_url(std::string_view rest)
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
        return std::unexpected(LocalReferenceRefusal::network);
    case FileUrlRefusal::malformed:
    case FileUrlRefusal::above_root:
        return std::unexpected(LocalReferenceRefusal::malformed);
    }
    return std::unexpected(LocalReferenceRefusal::malformed);
}

// 相対参照を文書のフォルダ（`\` か `/` で始まるならドライブの根）から解く。
[[nodiscard]] std::expected<FilePath, LocalReferenceRefusal>
relative_path(std::string_view reference, const std::optional<FilePath> &document)
{
    const std::string decoded = percent_decoded(url_without_suffix(reference));
    if (decoded.empty())
    {
        return std::unexpected(LocalReferenceRefusal::empty);
    }
    if (!document.has_value())
    {
        return std::unexpected(LocalReferenceRefusal::no_base);
    }
    const std::string_view base = document.value().text();
    if (network_prefix(base))
    {
        return std::unexpected(LocalReferenceRefusal::network);
    }
    if (!drive_absolute_path(base))
    {
        return std::unexpected(LocalReferenceRefusal::no_base);
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
[[nodiscard]] std::expected<FilePath, LocalReferenceRefusal> schemed(std::string_view reference,
                                                                     std::size_t length)
{
    const std::string_view scheme = reference.substr(0, length);
    if (ascii_equal_ignoring_case(scheme, "http") || ascii_equal_ignoring_case(scheme, "https"))
    {
        return std::unexpected(LocalReferenceRefusal::remote);
    }
    if (ascii_equal_ignoring_case(scheme, "data"))
    {
        return std::unexpected(LocalReferenceRefusal::data);
    }
    if (ascii_equal_ignoring_case(scheme, "file"))
    {
        return file_url(reference.substr(length + 1U));
    }
    return std::unexpected(LocalReferenceRefusal::unknown_scheme);
}
} // namespace

std::expected<FilePath, LocalReferenceRefusal>
resolve_local_reference(std::string_view reference, const std::optional<FilePath> &document)
{
    if (reference.empty())
    {
        return std::unexpected(LocalReferenceRefusal::empty);
    }
    if (drive_absolute_path(reference))
    {
        return local_path(reference);
    }
    if (network_prefix(reference))
    {
        return std::unexpected(LocalReferenceRefusal::network);
    }
    if (const auto length = uri_scheme_length(reference); length.has_value())
    {
        return schemed(reference, length.value());
    }
    if (reference.size() >= drive_length && ascii_letter(reference[0]) && reference[1] == ':')
    {
        return std::unexpected(LocalReferenceRefusal::malformed);
    }
    return relative_path(reference, document);
}
} // namespace nenenib::core
