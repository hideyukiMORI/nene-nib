#include "AbsolutePath.hpp"

#include "NativePath.hpp"
#include "Utf16.hpp"

#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
std::optional<core::FilePath> absolute_file_path(const std::wstring &path)
{
    // 絶対にする道は native_path と同じ full_path の 1 本（予約名を装置に写さない・#1805）。
    const std::wstring full = full_path(path);
    if (full.empty())
    {
        return std::nullopt;
    }
    // 変換できない経路（対にならないサロゲート）は空になり、FilePath::parse が拒む。
    auto parsed = core::FilePath::parse(core::to_utf8(full).value_or(std::string{}));
    if (!parsed)
    {
        return std::nullopt;
    }
    return std::move(parsed).value();
}

bool rooted_path_text(std::string_view text) noexcept
{
    const bool drive = text.size() >= 3 && text[1] == ':' && (text[2] == '\\' || text[2] == '/');
    return drive || text.starts_with("\\\\");
}
} // namespace nenenib::adapters::win32
