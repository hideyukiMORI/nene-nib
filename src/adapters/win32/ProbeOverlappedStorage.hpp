#pragma once
#include "FileHandle.hpp"
#include "ProbeReadFailure.hpp"
#include "ProbeStoragePermit.hpp"
#include <array>
#include <cstddef>
#include <expected>
#include <memory>
namespace nenenib::adapters::win32
{
class ProbeOverlappedStorage final
{
  public:
    [[nodiscard]] static std::expected<std::shared_ptr<ProbeOverlappedStorage>, ProbeReadFailure>
    create(std::unique_ptr<FileHandle> file, std::size_t count);
    ProbeOverlappedStorage(const ProbeOverlappedStorage &) = delete;
    ProbeOverlappedStorage &operator=(const ProbeOverlappedStorage &) = delete;
    ProbeOverlappedStorage(ProbeOverlappedStorage &&) = delete;
    ProbeOverlappedStorage &operator=(ProbeOverlappedStorage &&) = delete;
    ProbeOverlappedStorage(ProbeStoragePermit, std::unique_ptr<FileHandle> file,
                           std::unique_ptr<FileHandle> event, std::size_t count);

  private:
    friend class ProbeNativeReadTransport;
    friend class ProbeScriptReadTransport;
    friend class ProbeReadControl;
    std::unique_ptr<FileHandle> file_;
    std::unique_ptr<FileHandle> event_;
    OVERLAPPED overlapped_{};
    std::array<char, 64> buffer_{};
    std::size_t count_;
};
} // namespace nenenib::adapters::win32
