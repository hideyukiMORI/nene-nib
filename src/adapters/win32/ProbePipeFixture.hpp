#pragma once
#include "FileHandle.hpp"
#include "ProbeReadFailure.hpp"
#include <expected>
#include <memory>
#include <string_view>
namespace nenenib::adapters::win32
{
class ProbePipeFixture final
{
  public:
    [[nodiscard]] static std::expected<ProbePipeFixture, ProbeReadFailure> create();
    ProbePipeFixture(const ProbePipeFixture &) = delete;
    ProbePipeFixture &operator=(const ProbePipeFixture &) = delete;
    ProbePipeFixture(ProbePipeFixture &&) = default;
    ProbePipeFixture &operator=(ProbePipeFixture &&) = default;
    [[nodiscard]] std::unique_ptr<FileHandle> take_reader()
    {
        return std::move(reader_);
    }
    [[nodiscard]] bool write(std::string_view bytes);
    void close_writer() noexcept
    {
        writer_.reset();
    }

  private:
    ProbePipeFixture(std::unique_ptr<FileHandle> reader, std::unique_ptr<FileHandle> writer)
        : reader_(std::move(reader)), writer_(std::move(writer))
    {
    }
    std::unique_ptr<FileHandle> reader_, writer_;
};
} // namespace nenenib::adapters::win32
