#pragma once
namespace nenenib::adapters::win32
{
class ProbeStoragePermit final
{
  private:
    friend class ProbeOverlappedStorage;
    ProbeStoragePermit() = default;
};
} // namespace nenenib::adapters::win32
