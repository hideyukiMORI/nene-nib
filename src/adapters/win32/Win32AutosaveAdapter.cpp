#include "Win32AutosaveAdapter.hpp"

#include "AutosaveCodec.hpp"
#include "AutosaveFolder.hpp"
#include "LocalSettingsPath.hpp"

#include <windows.h>

#include <span>
#include <stop_token>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::AutosaveFailure;

// 窓が閉じていくときに走っている仕事を待つ上限（ADR 0137 の決定 5）。超えたら消せるものだけ消す。
constexpr DWORD clear_wait_milliseconds = 5000;

// 仕事の外枠。閉じた後と止める合図の後は何もしない。終われば成否を id つきで棚に積む。
template <typename Work>
void run_job(AutosaveShelf &shelf, application::AutosaveId id, const std::stop_token &stop,
             Work work)
{
    if (stop.stop_requested() || !shelf.begin())
    {
        return;
    }
    const std::expected<void, Failure> done = work();
    shelf.finish(application::AutosaveOutcome{
        id, done.has_value() ? std::nullopt : std::optional<Failure>{done.error()}});
    shelf.end();
}
} // namespace

std::optional<core::FilePath> local_autosave_root()
{
    return beside_local_settings("autosave");
}

Win32AutosaveAdapter::Win32AutosaveAdapter(Win32Worker &worker, application::FilePort &files,
                                           std::optional<core::FilePath> root)
    : worker_(worker), files_(files), root_(std::move(root)),
      shelf_(std::make_shared<AutosaveShelf>())
{
}

// 走っている仕事は棚を持ったまま終わる。合図はもう呼ばない。フォルダは消さない（正常に閉じた
// ときだけ clear が消す）。
Win32AutosaveAdapter::~Win32AutosaveAdapter()
{
    static_cast<void>(shelf_->close(0));
}

void Win32AutosaveAdapter::bind(std::function<void()> finished)
{
    shelf_->bind(std::move(finished));
}

void Win32AutosaveAdapter::store(application::AutosaveSnapshot snapshot)
{
    if (!shelf_->open())
    {
        return;
    }
    worker_.post(
        [shelf = shelf_, &files = files_, root = root_,
         snapshot = std::move(snapshot)](const std::stop_token &stop)
        {
            run_job(*shelf, snapshot.id, stop,
                    [&] { return autosave_stored(files, root, snapshot); });
        });
}

void Win32AutosaveAdapter::discard(application::AutosaveId id)
{
    if (!root_.has_value() || !shelf_->open())
    {
        return;
    }
    worker_.post(
        [shelf = shelf_, &files = files_,
         folder = autosave_folder(root_.value(), GetCurrentProcessId()),
         id](const std::stop_token &stop)
        { run_job(*shelf, id, stop, [&] { return autosave_dropped(files, folder, id); }); });
}

std::vector<application::AutosaveOutcome> Win32AutosaveAdapter::collect()
{
    return shelf_->take();
}

void Win32AutosaveAdapter::clear()
{
    static_cast<void>(shelf_->close(clear_wait_milliseconds));
    if (root_.has_value())
    {
        remove_autosave_folder(autosave_folder(root_.value(), GetCurrentProcessId()));
    }
}

std::expected<std::vector<application::AutosaveRemnant>, Failure> Win32AutosaveAdapter::remnants()
{
    if (!root_.has_value())
    {
        return std::unexpected(Failure::location_unavailable);
    }
    return autosave_remnants_in(files_, root_.value(), GetCurrentProcessId());
}

std::expected<std::vector<std::byte>, Failure>
Win32AutosaveAdapter::read(const application::AutosaveRemnant &remnant)
{
    const auto folder =
        root_.has_value() ? autosave_remnant_folder(root_.value(), remnant) : std::nullopt;
    if (!folder.has_value())
    {
        return std::unexpected(Failure::unreadable);
    }
    const auto body = core::FilePath::parse(folder.value() + "/" + autosave_body_name(remnant.id));
    if (!body)
    {
        return std::unexpected(Failure::unreadable);
    }
    const auto bytes = files_.read(body.value(), maximum_autosave_body_bytes);
    if (!bytes)
    {
        return std::unexpected(Failure::unreadable);
    }
    const auto view = std::as_bytes(std::span{bytes.value()});
    return std::vector<std::byte>(view.begin(), view.end());
}

void Win32AutosaveAdapter::forget(const application::AutosaveRemnant &remnant)
{
    const auto folder =
        root_.has_value() ? autosave_remnant_folder(root_.value(), remnant) : std::nullopt;
    if (folder.has_value())
    {
        static_cast<void>(autosave_dropped(files_, folder.value(), remnant.id));
    }
}
} // namespace nenenib::adapters::win32
