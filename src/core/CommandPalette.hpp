#pragma once

#include "CommandChoice.hpp"
#include "CommandLine.hpp"
#include "CommandPaletteSource.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::core
{
class CommandPalette final
{
  public:
    [[nodiscard]] static CommandPalette opened(ThemeCatalog themes = ThemeCatalog::builtins());
    // 開いているタブの一覧（ADR 0057 の決定 7）。tabs は帯の順の候補、active はアクティブの位置で、
    // 開いた直後の入力は空・選ばれているのはアクティブの行。
    [[nodiscard]] static CommandPalette opened_tabs(std::vector<CommandChoice> tabs,
                                                    std::size_t active,
                                                    ThemeCatalog themes = ThemeCatalog::builtins());
    [[nodiscard]] CommandPaletteSource source() const noexcept;
    [[nodiscard]] const CommandLine &input() const noexcept;
    [[nodiscard]] std::vector<CommandChoice> choices() const;
    [[nodiscard]] std::size_t selected() const noexcept;
    [[nodiscard]] std::expected<CommandPalette, ExFailure> inserted(std::string_view text) const;
    [[nodiscard]] CommandPalette edited(CommandEdit edit) const;
    [[nodiscard]] CommandPalette selected_at(std::size_t index) const;
    [[nodiscard]] std::expected<CommandPalette, ExFailure> filled(std::string_view command) const;

  private:
    CommandPalette(CommandLine input, std::size_t selected, CommandPaletteSource source,
                   std::vector<CommandChoice> tabs);
    [[nodiscard]] CommandPalette moved(CommandEdit direction) const;
    [[nodiscard]] CommandPalette with_input(CommandLine input, std::size_t selected) const;
    CommandLine input_;
    std::size_t selected_;
    CommandPaletteSource source_;
    // タブの一覧の候補（帯の順）。source_ が commands のときは空。
    std::vector<CommandChoice> tabs_;
};
} // namespace nenenib::core
