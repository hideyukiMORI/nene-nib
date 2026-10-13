#include "ProbeReadOwner.hpp"
#include <stop_token>
#include <utility>
namespace nenenib::adapters::win32
{
ProbeReadOwner::ProbeReadOwner(std::shared_ptr<ProbeReadControl> control,
                               std::shared_ptr<ProbeOverlappedStorage> storage)
    : control_(std::move(control)), storage_(std::move(storage))
{
}
std::expected<ProbeReadOwner, ProbeReadFailure>
ProbeReadOwner::create(std::unique_ptr<FileHandle> file, std::size_t count,
                       std::shared_ptr<ProbeReadTransport> transport)
{
    if (!transport)
    {
        return std::unexpected(ProbeReadFailure::create_failed);
    }
    auto storage = ProbeOverlappedStorage::create(std::move(file), count);
    if (!storage)
    {
        return std::unexpected(storage.error());
    }
    auto control = std::make_shared<ProbeReadControl>(std::move(transport));
    control->storage_ = storage.value();
    control->observation_ = storage.value();
    return ProbeReadOwner(std::move(control), std::move(storage).value());
}
std::expected<void, ProbeReadFailure> ProbeReadOwner::run(Win32Worker &worker)
{
    if (!storage_)
    {
        return std::unexpected(ProbeReadFailure::consumed);
    }
    worker.post(
        [control = control_, capture = std::move(storage_)](const std::stop_token &stop) mutable
        { run_job(control, std::move(capture), stop); });
    return {};
}
void ProbeReadOwner::run_job(std::shared_ptr<ProbeReadControl> control,
                             std::shared_ptr<ProbeOverlappedStorage> storage,
                             const std::stop_token &stop)
{
    {
        const std::stop_callback callback(stop,
                                          [control] { static_cast<void>(control->cancel()); });
        control->execute(storage);
    }
    storage.reset();
    control->transport_->hook(ProbeReadHook::finished);
}
std::expected<ProbeReadTicket, ProbeReadFailure> ProbeReadOwner::ticket() const
{
    if (!control_)
    {
        return std::unexpected(ProbeReadFailure::consumed);
    }
    return ProbeReadTicket(control_);
}
} // namespace nenenib::adapters::win32
