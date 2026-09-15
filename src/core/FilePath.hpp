#pragma once

#include "TextFailure.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::core
{
// ファイルの経路（UTF-8・検証済み）。生成経路は parse ただ 1 つ（CPP-007 / ADR 0010 の決定 11）。
// 絶対経路にするのも実際に開くのも adapters の仕事で、core は値として運ぶだけ（ARC-007）。
class FilePath final
{
  public:
    [[nodiscard]] static std::expected<FilePath, TextFailure> parse(std::string_view text);
    [[nodiscard]] std::string_view text() const noexcept;
    // 最後の '\' または '/' の後ろ。区切りが無ければ全体を返す。
    [[nodiscard]] std::string_view file_name() const noexcept;

  private:
    explicit FilePath(std::string text);
    std::string text_;
};

[[nodiscard]] bool operator==(const FilePath &left, const FilePath &right) noexcept;
} // namespace nenenib::core
