#pragma once
#include "ProbeReadTicket.hpp"
#include "Win32Worker.hpp"
#include <memory>
namespace nenenib::adapters::win32
{
class ProbeReadOwner final
{
  public:
    [[nodiscard]] static std::expected<ProbeReadOwner, ProbeReadFailure>
    create(std::unique_ptr<FileHandle> file, std::size_t count,
           std::shared_ptr<ProbeReadTransport> transport);
    ProbeReadOwner(const ProbeReadOwner &) = delete;
    ProbeReadOwner &operator=(const ProbeReadOwner &) = delete;
    ProbeReadOwner(ProbeReadOwner &&) = default;
    ProbeReadOwner &operator=(ProbeReadOwner &&) = default;
    [[nodiscard]] std::expected<void, ProbeReadFailure> run(Win32Worker &worker);
    [[nodiscard]] std::expected<ProbeReadTicket, ProbeReadFailure> ticket() const;

  private:
    static void run_job(std::shared_ptr<ProbeReadControl> control,
                        std::shared_ptr<ProbeOverlappedStorage> storage,
                        const std::stop_token &stop);
    ProbeReadOwner(std::shared_ptr<ProbeReadControl> control,
                   std::shared_ptr<ProbeOverlappedStorage> storage);
    std::shared_ptr<ProbeReadControl> control_;
    std::shared_ptr<ProbeOverlappedStorage> storage_;
};
} // namespace nenenib::adapters::win32
