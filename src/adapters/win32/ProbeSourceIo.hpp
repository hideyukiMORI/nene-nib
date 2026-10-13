#pragma once
#include "FileFailure.hpp"
#include "ProbeIoFault.hpp"
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <vector>
#include <windows.h>
namespace nenenib::adapters::win32
{
// 限定実験の同期I/O境界。読込の全量判定はCompleteFileReadだけが持つ。
class ProbeSourceIo final
{
  public:
    void script_reads(std::vector<ProbeIoFault> faults);
    void script_writes(std::vector<ProbeIoFault> faults);
    void script_size(std::size_t declared_size);
    [[nodiscard]] std::expected<std::size_t, application::FileFailure> size(HANDLE file) const;
    [[nodiscard]] std::expected<std::size_t, application::FileFailure>
    read(HANDLE file, std::span<char> target) noexcept;
    [[nodiscard]] std::expected<std::size_t, application::FileFailure>
    write(HANDLE file, std::span<const char> bytes) noexcept;

  private:
    std::vector<ProbeIoFault> reads_;
    std::vector<ProbeIoFault> writes_;
    std::size_t read_at_ = 0;
    std::size_t write_at_ = 0;
    std::optional<std::size_t> size_;
};
} // namespace nenenib::adapters::win32
