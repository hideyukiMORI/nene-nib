#pragma once

#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "FilePort.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
using nenenib::application::FileFailure;
using nenenib::application::FilePort;
using nenenib::core::FilePath;
using Bytes = std::expected<std::string, FileFailure>;

// 偽のファイル。OS に触れずに「読めた」「読めない」「書けない」を作り分ける（ARC-007）。
class ScriptedFiles final : public FilePort
{
  public:
    void hold(Bytes content)
    {
        content_ = std::move(content);
    }

    void refuse_writes(FileFailure failure)
    {
        write_failure_ = failure;
    }

    [[nodiscard]] const std::string &written() const noexcept
    {
        return written_;
    }

    [[nodiscard]] const std::string &written_path() const noexcept
    {
        return written_path_;
    }

    [[nodiscard]] const std::string &read_path() const noexcept
    {
        return read_path_;
    }

    // application が渡してきた上限。値の正本が 1 つであることをテストが見る（ARC-001）。
    [[nodiscard]] std::size_t read_limit() const noexcept
    {
        return read_limit_;
    }

    [[nodiscard]] Bytes read(const FilePath &path, std::size_t maximum_bytes) override
    {
        read_path_ = std::string(path.text());
        read_limit_ = maximum_bytes;
        return content_;
    }

    [[nodiscard]] std::expected<void, FileFailure> write(const FilePath &path,
                                                         std::string_view bytes) override
    {
        if (write_failure_.has_value())
        {
            return std::unexpected(write_failure_.value());
        }
        written_path_ = std::string(path.text());
        written_ = std::string(bytes);
        return {};
    }

  private:
    Bytes content_{std::unexpected(FileFailure::not_found)};
    std::optional<FileFailure> write_failure_;
    std::string written_;
    std::string written_path_;
    std::string read_path_;
    std::size_t read_limit_ = 0;
};
} // namespace nenenib::tests
