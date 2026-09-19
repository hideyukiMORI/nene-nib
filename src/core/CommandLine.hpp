#pragma once

#include "CommandEdit.hpp"
#include "ExFailure.hpp"
#include "Offset.hpp"
#include "ThemeCatalog.hpp"

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 本文とは独立した、検証済みの一行入力。編集位置は既存の UTF-8 境界だけを通る。
class CommandLine final
{
  public:
    [[nodiscard]] static CommandLine empty(ThemeCatalog themes = ThemeCatalog::builtins());
    [[nodiscard]] const ThemeCatalog &catalog() const & noexcept;
    const ThemeCatalog &catalog() const && = delete;
    [[nodiscard]] std::string_view text() const noexcept;
    [[nodiscard]] Offset caret() const noexcept;
    [[nodiscard]] std::vector<std::string> completions() const;
    [[nodiscard]] std::optional<std::size_t> completion_index() const noexcept;
    [[nodiscard]] std::expected<CommandLine, ExFailure> inserted(std::string_view text) const;
    [[nodiscard]] CommandLine edited(CommandEdit edit) const;

  private:
    CommandLine(std::string text, Offset caret, ThemeCatalog themes);
    [[nodiscard]] CommandLine completed(CommandEdit direction) const;

    std::string text_;
    Offset caret_;
    std::optional<std::string> completion_seed_;
    std::size_t completion_index_ = 0;
    ThemeCatalog themes_;
};
} // namespace nenenib::core
