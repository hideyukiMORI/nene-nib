#pragma once

#include "CommandChoice.hpp"
#include "CommandLine.hpp"

namespace nenenib::core
{
class CommandPalette final
{
  public:
    [[nodiscard]] static CommandPalette opened();
    [[nodiscard]] const CommandLine &input() const noexcept;
    [[nodiscard]] std::vector<CommandChoice> choices() const;
    [[nodiscard]] std::size_t selected() const noexcept;
    [[nodiscard]] std::expected<CommandPalette, ExFailure> inserted(std::string_view text) const;
    [[nodiscard]] CommandPalette edited(CommandEdit edit) const;
    [[nodiscard]] CommandPalette selected_at(std::size_t index) const;
    [[nodiscard]] std::expected<CommandPalette, ExFailure> filled(std::string_view command) const;

  private:
    CommandPalette(CommandLine input, std::size_t selected);
    [[nodiscard]] CommandPalette moved(CommandEdit direction) const;
    CommandLine input_;
    std::size_t selected_;
};
} // namespace nenenib::core
