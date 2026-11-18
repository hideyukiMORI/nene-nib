#include "LinkDestination.hpp"

#include "AsciiCase.hpp"
#include "FilePath.hpp"
#include "LinkKind.hpp"
#include "LocalReference.hpp"
#include "UnlistedExtensions.hpp"
#include "UriScheme.hpp"
#include "Utf8.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
[[nodiscard]] bool same_ignoring_case(std::string_view left, std::string_view right) noexcept
{
    return left.size() == right.size() && ascii_prefix_ignoring_case(left, right);
}

[[nodiscard]] LinkDestination spelled(LinkKind kind, std::string_view target)
{
    return LinkDestination{.kind = kind, .text = std::string(target)};
}

// ローカルの参照（scheme の無い宛先と file: の URL）。解き方は画像と同じ 1 本。
[[nodiscard]] LinkDestination local(std::string_view target,
                                    const std::optional<FilePath> &document)
{
    const auto path = resolve_local_reference(target, document);
    if (!path || !folder_lists(path.value().file_name()))
    {
        return spelled(LinkKind::refused, target);
    }
    return spelled(LinkKind::document, path.value().text());
}
} // namespace

LinkDestination link_destination(std::string_view target, const std::optional<FilePath> &document)
{
    if (target.empty() || has_control_character(target))
    {
        return spelled(LinkKind::refused, target);
    }
    if (target.front() == '#')
    {
        return spelled(LinkKind::heading, target.substr(1));
    }
    const auto length = uri_scheme_length(target);
    if (!length.has_value())
    {
        return local(target, document);
    }
    const std::string_view scheme = target.substr(0, length.value());
    if (same_ignoring_case(scheme, "http") || same_ignoring_case(scheme, "https"))
    {
        return spelled(LinkKind::web, target);
    }
    if (same_ignoring_case(scheme, "mailto"))
    {
        return spelled(LinkKind::mail, target);
    }
    if (same_ignoring_case(scheme, "file"))
    {
        return local(target, document);
    }
    return spelled(LinkKind::refused, target);
}
} // namespace nenenib::core
