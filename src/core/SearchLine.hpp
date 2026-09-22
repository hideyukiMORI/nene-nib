#pragma once

#include "CommandEdit.hpp"
#include "ExFailure.hpp"
#include "InputText.hpp"
#include "Offset.hpp"
#include "VimSearchDirection.hpp"

#include <expected>
#include <string_view>

namespace nenenib::core
{
// 検索の入力行（ADR 0032 の決定 1）。本文とは独立した、検証済みの 1 行。編集の規則は
// InputText の純関数で Ex の CommandLine と共用し、補完と ThemeCatalog は持たない。
// 生成経路は opened ただ 1 つで、位置は UTF-8 の境界だけを通る（CPP-007）。
class SearchLine final
{
  public:
    [[nodiscard]] static SearchLine opened(VimSearchDirection direction);
    [[nodiscard]] std::string_view text() const noexcept;
    [[nodiscard]] Offset caret() const noexcept;
    [[nodiscard]] VimSearchDirection direction() const noexcept;
    [[nodiscard]] std::expected<SearchLine, ExFailure> inserted(std::string_view text) const;
    [[nodiscard]] SearchLine edited(CommandEdit edit) const;

  private:
    SearchLine(InputText input, VimSearchDirection direction);

    InputText input_;
    VimSearchDirection direction_;
};
} // namespace nenenib::core
