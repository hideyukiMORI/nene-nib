#pragma once

#include "CommandEdit.hpp"
#include "ExFailure.hpp"
#include "FontFamilies.hpp"
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
    [[nodiscard]] const FontFamilies &fonts() const & noexcept;
    const FontFamilies &fonts() const && = delete;
    // 同じ入力・キャレット・補完の位置で、字体の一覧だけを入れた行（#597・D58）。
    [[nodiscard]] CommandLine with_fonts(FontFamilies fonts) const;
    [[nodiscard]] std::string_view text() const noexcept;
    [[nodiscard]] Offset caret() const noexcept;
    [[nodiscard]] std::vector<std::string> completions() const;
    // 巡回で選ばれている候補（ADR 0150 の決定 3）。巡回していない・巡回が打った元の文字に
    // 戻っている間は空。
    [[nodiscard]] std::optional<std::size_t> completion_index() const noexcept;
    // 補完の面の見えている窓の先頭（ADR 0150 の決定 1・Ctrl+P の面と同じ list_window_first）。
    [[nodiscard]] std::size_t completion_first() const noexcept;
    [[nodiscard]] std::size_t completion_rows() const noexcept;
    // 補完の面の見えている行数を替えた行（application が窓の寸法から決めて渡す・上限は
    // command_completion_row_limit）。選ばれている候補が見えるように先頭を寄せ直す。
    [[nodiscard]] CommandLine with_completion_rows(std::size_t rows) const;
    [[nodiscard]] std::expected<CommandLine, ExFailure> inserted(std::string_view text) const;
    [[nodiscard]] CommandLine edited(CommandEdit edit) const;

  private:
    CommandLine(std::string text, Offset caret, ThemeCatalog themes, FontFamilies fonts);
    // Tab / Shift+Tab（ADR 0150 の決定 3）。巡回していなければ候補の共通の接頭辞まで埋め、
    // これ以上埋まらなければ巡回に入る。巡回の端の次は打った元の文字（completion_seed_）。
    [[nodiscard]] CommandLine completed(CommandEdit direction) const;
    [[nodiscard]] CommandLine cycled(const std::vector<std::string> &candidates,
                                     CommandEdit direction) const;

    std::string text_;
    Offset caret_;
    // 巡回の元の入力。値があれば巡回の中で、候補はこの文字列から作る。
    std::optional<std::string> completion_seed_;
    std::optional<std::size_t> completion_index_;
    std::size_t completion_first_ = 0;
    std::size_t completion_rows_;
    ThemeCatalog themes_;
    FontFamilies fonts_;
};
} // namespace nenenib::core
