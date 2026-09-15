#pragma once

#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "FilePort.hpp"

#include <windows.h>

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
// ファイルを触る唯一の場所（ARC-003 / ARC-007 / ADR 0010 の決定 1・6）。文字コードは知らない。
// 書き込みは同じフォルダの一時ファイルに書いてから置き換えるので、途中で落ちても元は残る。
// 読める上限は application が引数で渡す。書き込みに上限は無い（入力を失わせない）。
class Win32FileAdapter final : public application::FilePort
{
  public:
    [[nodiscard]] std::expected<std::string, application::FileFailure>
    read(const core::FilePath &path, std::size_t maximum_bytes) override;
    [[nodiscard]] std::expected<void, application::FileFailure>
    write(const core::FilePath &path, std::string_view bytes) override;
};
} // namespace nenenib::adapters::win32
