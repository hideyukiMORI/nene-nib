#pragma once

#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "FileWriteMode.hpp"

#include <cstddef>
#include <expected>
#include <span>
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
    // 既存の保存は replace。実際の書き込みは mode 付きの一つだけ（ADR 0067）。
    [[nodiscard]] std::expected<void, FileFailure> write(const core::FilePath &path,
                                                         std::string_view bytes)
    {
        return write(path, bytes, FileWriteMode::replace);
    }
    [[nodiscard]] std::expected<void, FileFailure> write(const core::FilePath &path,
                                                         std::string_view bytes, FileWriteMode mode)
    {
        return write(path, std::span<const std::string_view>(&bytes, 1), mode);
    }
    // 断片を順に連結したbytesを書く唯一の実装口（ADR 0119）。空列/空断片も許す。
    // 列と参照先は呼出し中だけ借用し、実装は返却後に保持しない。
    [[nodiscard]] virtual std::expected<void, FileFailure>
    write(const core::FilePath &path, std::span<const std::string_view> parts,
          FileWriteMode mode) = 0;
    [[nodiscard]] virtual std::expected<core::FilePath, FileFailure>
    resolve(const core::FilePath &path) = 0;
    // 2 つの経路が同じファイルを指すか（ADR 0056 の決定 5）。比べ方は OS の規則で、adapters が
    // 決める（Windows は大文字と小文字を区別しない序数比較）。
    [[nodiscard]] virtual bool same_file(const core::FilePath &left,
                                         const core::FilePath &right) const = 0;
};
} // namespace nenenib::application
