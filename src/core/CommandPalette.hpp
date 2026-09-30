#pragma once

#include "CommandChoice.hpp"
#include "CommandLine.hpp"
#include "PaletteScope.hpp"

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// Ctrl+P の面（ADR 0060 の決定 1・5）。候補の列は開くときに受け取って共有で持ち（1 打鍵で写すのは
// 参照だけ）、入力の先頭の記号がどの出どころを見せるかを決める。
class CommandPalette final
{
  public:
    // 開くのはこの 1 本。input は最初の入力、selected は最初の choices() の中の位置で、範囲の外
    // なら先頭。
    [[nodiscard]] static CommandPalette opened(std::vector<CommandChoice> entries,
                                               std::string_view input, std::size_t selected,
                                               ThemeCatalog themes = ThemeCatalog::builtins());
    [[nodiscard]] PaletteScope scope() const noexcept;
    [[nodiscard]] const CommandLine &input() const noexcept;
    [[nodiscard]] std::vector<CommandChoice> choices() const;
    [[nodiscard]] std::size_t selected() const noexcept;
    [[nodiscard]] std::expected<CommandPalette, ExFailure> inserted(std::string_view text) const;
    [[nodiscard]] CommandPalette edited(CommandEdit edit) const;
    [[nodiscard]] CommandPalette selected_at(std::size_t index) const;
    [[nodiscard]] std::expected<CommandPalette, ExFailure> filled(std::string_view command) const;

  private:
    CommandPalette(CommandLine input, std::size_t selected,
                   std::shared_ptr<const std::vector<CommandChoice>> entries);
    [[nodiscard]] CommandPalette moved(CommandEdit direction) const;
    [[nodiscard]] CommandPalette with_input(CommandLine input, std::size_t selected) const;
    CommandLine input_;
    std::size_t selected_;
    // 開くときに受け取った候補の列（ADR 0060 の決定 1）。入力が変わっても同じ列を指す。
    std::shared_ptr<const std::vector<CommandChoice>> entries_;
};
} // namespace nenenib::core
