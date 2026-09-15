#pragma once

#include "FileFailure.hpp"
#include "FilePath.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace nenenib::application
{
// ファイルを触る唯一の入口（ADR 0010 の決定 1）。扱うのはバイト列だけで、文字コードは知らない。
// 実装は src/adapters/win32 だけが持つ（ARC-003 / ARC-007）。
// 同期で読める上限は application が決めて read の引数で渡す。値の正本は 1 つ（ARC-001 / 決定 12）。
class FilePort
{
  public:
    FilePort() = default;
    virtual ~FilePort() = default;
    FilePort(const FilePort &) = delete;
    FilePort(FilePort &&) = delete;
    FilePort &operator=(const FilePort &) = delete;
    FilePort &operator=(FilePort &&) = delete;

    [[nodiscard]] virtual std::expected<std::string, FileFailure>
    read(const core::FilePath &path, std::size_t maximum_bytes) = 0;
    [[nodiscard]] virtual std::expected<void, FileFailure> write(const core::FilePath &path,
                                                                 std::string_view bytes) = 0;
};
} // namespace nenenib::application
