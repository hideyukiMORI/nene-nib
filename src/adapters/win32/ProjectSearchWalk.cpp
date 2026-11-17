#include "ProjectSearchWalk.hpp"

#include "FindHandle.hpp"
#include "NativePath.hpp"
#include "ProjectChunkHits.hpp"
#include "ProjectHits.hpp"
#include "ProjectSearchBatch.hpp"
#include "ProjectSearchEntries.hpp"
#include "ProjectSearchProgress.hpp"
#include "ProjectSearchRules.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using application::ProjectSearchPhase;
using Ended = std::optional<ProjectSearchPhase>;

[[nodiscard]] bool ends_with_separator(std::string_view folder) noexcept
{
    return folder.ends_with('\\') || folder.ends_with('/');
}

// フォルダが区切りで終わっていれば（C:\ など）区切りを足さない（FolderListing と同じ形）。
[[nodiscard]] std::optional<core::FilePath> joined(const core::FilePath &folder,
                                                   std::string_view name)
{
    std::string path(folder.text());
    if (!ends_with_separator(path))
    {
        path += '\\';
    }
    path += name;
    auto parsed = core::FilePath::parse(path);
    if (!parsed)
    {
        return std::nullopt;
    }
    return std::move(parsed).value();
}

// Win32 のファイルの関数へ渡す形（長い経路は `\\?\` を付ける・NativePath の 1 本）。
[[nodiscard]] std::optional<std::wstring> native_of(std::string_view path)
{
    const auto wide = core::to_utf16(path);
    if (!wide || wide.value().empty())
    {
        return std::nullopt;
    }
    return native_path(wide.value());
}

[[nodiscard]] bool is_folder(const WIN32_FIND_DATAW &data) noexcept
{
    return (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

[[nodiscard]] std::uint64_t size_of(const WIN32_FIND_DATAW &data) noexcept
{
    return (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32U) | data.nFileSizeLow;
}

// 項目の名前（UTF-8）。`.` `..` と UTF-8 にできない名前は値なし。
[[nodiscard]] std::optional<std::string> entry_name(const WIN32_FIND_DATAW &data)
{
    const std::wstring_view wide(static_cast<const wchar_t *>(data.cFileName));
    if (wide == L"." || wide == L"..")
    {
        return std::nullopt;
    }
    auto name = core::to_utf8(wide);
    if (!name)
    {
        return std::nullopt;
    }
    return std::move(name).value();
}

// 空のルート（C:\ など）には "." も ".." も無く、FindFirstFileExW は ERROR_FILE_NOT_FOUND で返る。
[[nodiscard]] bool empty_folder(DWORD error) noexcept
{
    return error == ERROR_FILE_NOT_FOUND;
}
} // namespace

ProjectSearchWalk::ProjectSearchWalk(application::ProjectSearchRequest request,
                                     std::uint64_t generation,
                                     std::shared_ptr<ProjectSearchShelf> shelf, Win32Worker &worker)
    : request_(std::move(request)), generation_(generation), shelf_(std::move(shelf)),
      worker_(&worker), folders_{request_.root}
{
}

void ProjectSearchWalk::run(const std::stop_token &stop)
{
    // 列で待っている間に古くなった要求は、I/O を呼ばずに戻る（ハンドルはここで放す）。
    if (!live(stop))
    {
        file_.reset();
        return;
    }
    Ended ended;
    // 確保の失敗は仕事の中で受けて値に変える（DocumentChunkRead と同じ・core は投げない）。
    try
    {
        ended = step();
    }
    catch (const std::bad_alloc &)
    {
        ended = ProjectSearchPhase::failed;
    }
    if (ended.has_value())
    {
        file_.reset();
        static_cast<void>(send(ended.value()));
        return;
    }
    if (moved_ && !send(ProjectSearchPhase::searching))
    {
        file_.reset();
        return;
    }
    Win32Worker &worker = *worker_;
    worker.post([job = std::move(*this)](const std::stop_token &next) mutable { job.run(next); });
}

bool ProjectSearchWalk::live(const std::stop_token &stop) const
{
    return !stop.stop_requested() && shelf_->current(generation_);
}

std::optional<ProjectSearchPhase> ProjectSearchWalk::step()
{
    if (file_)
    {
        return read_chunk();
    }
    if (!files_.empty())
    {
        return open_next();
    }
    if (!folders_.empty())
    {
        return list_folder();
    }
    return ProjectSearchPhase::complete;
}

std::optional<ProjectSearchPhase> ProjectSearchWalk::list_folder()
{
    const core::FilePath folder = std::move(folders_.back());
    folders_.pop_back();
    const bool root = !std::exchange(root_listed_, true);
    const auto pattern = joined(folder, "*");
    const auto native = pattern.has_value() ? native_of(pattern.value().text()) : std::nullopt;
    if (!native.has_value())
    {
        return root ? Ended{ProjectSearchPhase::failed} : std::nullopt;
    }
    WIN32_FIND_DATAW data{};
    const FindHandle search(FindFirstFileExW(native.value().c_str(), FindExInfoBasic, &data,
                                             FindExSearchNameMatch, nullptr,
                                             FIND_FIRST_EX_LARGE_FETCH));
    if (!search.valid())
    {
        // 開けない下のフォルダ（拒否・長すぎる経路）は黙って飛ばす（数えない）。
        return root && !empty_folder(GetLastError()) ? Ended{ProjectSearchPhase::failed}
                                                     : std::nullopt;
    }
    std::vector<core::FilePath> found;
    const auto files_before = found_;
    do
    {
        take_entry(folder, data, found);
    } while (FindNextFileW(search.get(), &data) != 0);
    // 見つけた順に歩くので、末尾から取る列へは逆の順で積む。
    folders_.insert(folders_.end(), std::make_move_iterator(found.rbegin()),
                    std::make_move_iterator(found.rend()));
    // 見つけた数が変わったときだけ知らせる（ファイルの無いフォルダは batch を出さない）。
    moved_ = moved_ || found_ != files_before;
    return std::nullopt;
}

void ProjectSearchWalk::take_entry(const core::FilePath &folder, const WIN32_FIND_DATAW &data,
                                   std::vector<core::FilePath> &folders)
{
    const auto name = entry_name(data);
    if (!name.has_value())
    {
        return;
    }
    const bool walked = is_folder(data) &&
                        project_folder_walked(data.dwFileAttributes, data.dwReserved0) &&
                        !core::project_folder_skipped(name.value());
    const bool searched = !is_folder(data) && project_file_read(data.dwFileAttributes) &&
                          core::project_file_fits(size_of(data));
    if (!walked && !searched)
    {
        return;
    }
    auto path = joined(folder, name.value());
    if (!path.has_value())
    {
        return;
    }
    if (walked)
    {
        folders.push_back(std::move(path).value());
        return;
    }
    files_.push_back(std::move(path).value());
    ++found_;
}

std::optional<ProjectSearchPhase> ProjectSearchWalk::open_next()
{
    reading_ = std::move(files_.front());
    files_.pop_front();
    const auto native = native_of(reading_.value().text());
    if (!native.has_value())
    {
        skip_file();
        return std::nullopt;
    }
    file_ = std::make_unique<FileHandle>(
        CreateFileW(native.value().c_str(), GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                    FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (!file_->valid())
    {
        skip_file();
        return std::nullopt;
    }
    scan_ = core::ProjectScan{};
    first_chunk_ = true;
    return read_chunk();
}

std::optional<ProjectSearchPhase> ProjectSearchWalk::read_chunk()
{
    buffer_.resize(core::project_search_chunk_bytes);
    DWORD done = 0;
    if (ReadFile(file_->get(), buffer_.data(), static_cast<DWORD>(buffer_.size()), &done,
                 nullptr) == 0)
    {
        skip_file();
        return std::nullopt;
    }
    buffer_.resize(done);
    if (std::exchange(first_chunk_, false) && core::project_text_is_binary(buffer_))
    {
        skip_file();
        return std::nullopt;
    }
    if (buffer_.empty())
    {
        return end_file();
    }
    auto chunk = core::project_hits_in(scan_, buffer_, request_.pattern, room());
    scan_ = std::move(chunk.next);
    if (auto ended = add_hits(chunk.hits))
    {
        return ended;
    }
    // 塊より短く読めたらファイルの終わり（次の仕事で 0 バイトを読みに行かない）。
    return buffer_.size() < core::project_search_chunk_bytes ? end_file() : std::nullopt;
}

std::optional<ProjectSearchPhase> ProjectSearchWalk::end_file()
{
    file_.reset();
    const auto last = core::project_hits_finished(scan_, request_.pattern, room());
    scan_ = core::ProjectScan{};
    ++searched_;
    moved_ = true;
    return add_hits(last.hits);
}

void ProjectSearchWalk::skip_file()
{
    file_.reset();
    --found_;
    moved_ = true;
}

std::optional<ProjectSearchPhase>
ProjectSearchWalk::add_hits(const std::vector<core::ProjectHit> &hits)
{
    const auto count = reading_.has_value() ? std::min(hits.size(), room()) : 0;
    for (std::size_t index = 0; index < count && reading_.has_value(); ++index)
    {
        hits_.push_back(application::ProjectSearchHit{reading_.value(), hits.at(index)});
    }
    total_hits_ += count;
    moved_ = moved_ || count > 0;
    if (total_hits_ < core::project_search_hit_limit)
    {
        return std::nullopt;
    }
    // 上限に届いた。読みかけのファイルが残っていても数えずに終える。
    return ProjectSearchPhase::truncated;
}

std::size_t ProjectSearchWalk::room() const noexcept
{
    return core::project_search_hit_limit - std::min(total_hits_, core::project_search_hit_limit);
}

bool ProjectSearchWalk::send(ProjectSearchPhase phase)
{
    moved_ = false;
    return shelf_->deliver(generation_,
                           application::ProjectSearchBatch{
                               request_.ticket, std::exchange(hits_, {}),
                               application::ProjectSearchProgress{phase, searched_, found_}});
}
} // namespace nenenib::adapters::win32
