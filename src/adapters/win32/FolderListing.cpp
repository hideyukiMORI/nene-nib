#include "FolderListing.hpp"

#include "FindHandle.hpp"
#include "FolderBatch.hpp"
#include "Utf16.hpp"

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using application::FolderProgress;

constexpr DWORD skipped_attributes =
    FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;

[[nodiscard]] bool ends_with_separator(std::string_view folder) noexcept
{
    return folder.ends_with('\\') || folder.ends_with('/');
}

// フォルダが区切りで終わっていれば（C:\ など）区切りを足さない。
[[nodiscard]] std::string joined(std::string_view folder, std::string_view name)
{
    std::string path(folder);
    if (!ends_with_separator(folder))
    {
        path += '\\';
    }
    path += name;
    return path;
}

[[nodiscard]] std::optional<core::FilePath> listed_file(const core::FilePath &folder,
                                                        const WIN32_FIND_DATAW &data)
{
    if ((data.dwFileAttributes & skipped_attributes) != 0)
    {
        return std::nullopt;
    }
    const auto name =
        core::to_utf8(std::wstring_view(static_cast<const wchar_t *>(data.cFileName)));
    if (!name)
    {
        return std::nullopt;
    }
    auto path = core::FilePath::parse(joined(folder.text(), name.value()));
    if (!path)
    {
        return std::nullopt;
    }
    return std::move(path).value();
}

// 空のルート（C:\ など）には "." も ".." も無く、
// FindFirstFileExW は ERROR_FILE_NOT_FOUND で返る。
[[nodiscard]] FolderProgress unopened(DWORD error) noexcept
{
    return error == ERROR_FILE_NOT_FOUND ? FolderProgress::complete : FolderProgress::failed;
}

[[nodiscard]] FolderProgress ended(DWORD error) noexcept
{
    return error == ERROR_NO_MORE_FILES ? FolderProgress::complete : FolderProgress::failed;
}
} // namespace

FolderListing::FolderListing(application::FolderRequest request, std::uint64_t generation,
                             std::shared_ptr<FolderShelf> shelf)
    : request_(std::move(request)), generation_(generation), shelf_(std::move(shelf))
{
}

void FolderListing::run(const std::stop_token &stop)
{
    const auto pattern = core::to_utf16(joined(request_.folder.text(), "*"));
    if (!pattern)
    {
        finish(stop, FolderProgress::failed);
        return;
    }
    WIN32_FIND_DATAW data{};
    const FindHandle search(FindFirstFileExW(pattern.value().c_str(), FindExInfoBasic, &data,
                                             FindExSearchNameMatch, nullptr,
                                             FIND_FIRST_EX_LARGE_FETCH));
    if (!search.valid())
    {
        finish(stop, unopened(GetLastError()));
        return;
    }
    do
    {
        if (!live(stop))
        {
            return;
        }
        auto file = listed_file(request_.folder, data);
        if (file.has_value() && !take(std::move(file).value()))
        {
            return;
        }
    } while (FindNextFileW(search.get(), &data) != 0);
    finish(stop, ended(GetLastError()));
}

bool FolderListing::live(const std::stop_token &stop) const
{
    return !stop.stop_requested() && shelf_->current(generation_);
}

bool FolderListing::take(core::FilePath file)
{
    if (total_ == shelf_->file_limit())
    {
        // 上限を超える 1 件が見えたので、上限までを出して打ち切る。
        static_cast<void>(send(FolderProgress::truncated));
        return false;
    }
    // 満ちた batch は、次の 1 件が見えてから出す（最後の batch だけが complete になる）。
    if (files_.size() == shelf_->batch_files() && !send(FolderProgress::more))
    {
        return false;
    }
    files_.push_back(std::move(file));
    ++total_;
    return true;
}

bool FolderListing::send(FolderProgress progress)
{
    return shelf_->deliver(generation_, {request_.ticket, std::exchange(files_, {}), progress});
}

// 最後の batch を出す。止める合図か新しい世代が来ていたら出さない
// （取り消された I/O の失敗を failed にしない）。
void FolderListing::finish(const std::stop_token &stop, FolderProgress progress)
{
    if (live(stop))
    {
        static_cast<void>(send(progress));
    }
}
} // namespace nenenib::adapters::win32
