#pragma once

#include "CommandChoice.hpp"
#include "CommandLine.hpp"
#include "EditMode.hpp"
#include "PaletteScope.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace nenenib::core
{
// Ctrl+P の面（ADR 0060 の決定 1・5）。候補の列は開くときに受け取って共有で持ち（1 打鍵で写すのは
// 参照だけ）、入力の先頭の記号がどの出どころを見せるかを決める。絞り込みの結果は入力が変わった
// ときに 1 回だけ作って共有で持ち、選択を動かす道は同じ結果を読む（ADR 0062 の決定 1）。
class CommandPalette final
{
  public:
    // 開くのはこの 1 本。input は最初の入力で、選択は先頭（最初の選択は selected_at で動かす）。
    // mode は開いたときの編集モードで、`?` の操作の一覧がモードで使える操作を選ぶのに読む
    // （ADR 0078 の決定 11）。
    [[nodiscard]] static CommandPalette opened(std::vector<CommandChoice> entries,
                                               std::string_view input, EditMode mode,
                                               ThemeCatalog themes = ThemeCatalog::builtins());
    [[nodiscard]] PaletteScope scope() const noexcept;
    [[nodiscard]] const CommandLine &input() const noexcept;
    // 絞り込みの結果の件数。
    [[nodiscard]] std::size_t count() const noexcept;
    // 結果の index 番目の候補。範囲の外は無し。
    [[nodiscard]] std::optional<CommandChoice> choice_at(std::size_t index) const;
    // 結果の first 番目から最大 limit 件の写し。範囲の外は切り詰める。
    [[nodiscard]] std::vector<CommandChoice> rows(std::size_t first, std::size_t limit) const;
    // 2 つの面が同じ絞り込みの結果を共有しているか（作り直していないことの観測の口・中身は
    // 出さない）。
    [[nodiscard]] bool shares_result_with(const CommandPalette &other) const noexcept;
    [[nodiscard]] std::size_t selected() const noexcept;
    [[nodiscard]] std::expected<CommandPalette, ExFailure> inserted(std::string_view text) const;
    [[nodiscard]] CommandPalette edited(CommandEdit edit) const;
    [[nodiscard]] CommandPalette selected_at(std::size_t index) const;
    [[nodiscard]] std::expected<CommandPalette, ExFailure> filled(std::string_view command) const;
    // 候補の列の後ろへ more を足した面（ADR 0062 の決定 16）。前の列の順と位置は変えず、結果は
    // filtered の 1 本で作り直す。選択は伸ばす前に選んでいた候補と同じ候補（列の中の位置で探す）、
    // 伸ばす前の結果が空なら先頭、設定のコマンド（commands）は番号のまま。入力は変えない。
    // more が空なら同じ面を返す。
    [[nodiscard]] CommandPalette extended(std::vector<CommandChoice> more) const;

  private:
    using Entries = std::shared_ptr<const std::vector<CommandChoice>>;
    // 絞り込みの結果（ADR 0062 の決定 1）。ファイルの候補（files・tabs・history）は entries_ の
    // 中の位置の列で、候補の写しを作らない。設定のコマンド（commands）は palette_choices の値の列、
    // 操作の一覧（operations）は operation_choices の値の列。
    using Result = std::variant<std::vector<std::size_t>, std::vector<CommandChoice>>;
    CommandPalette(CommandLine input, std::size_t selected, EditMode mode, Entries entries,
                   std::shared_ptr<const Result> result);
    // 入力が変わる道（opened・inserted・edited・filled）はこの 1 本で結果を 1 回作る。選択は先頭。
    [[nodiscard]] static CommandPalette filtered(CommandLine input, EditMode mode, Entries entries);
    [[nodiscard]] static Result result_of(const CommandLine &input, EditMode mode,
                                          const std::vector<CommandChoice> &entries);
    // 選択だけを動かす道（moved・selected_at）はこの 1 本で、前の結果をそのまま共有する。
    [[nodiscard]] CommandPalette reselected(std::size_t selected) const;
    [[nodiscard]] CommandPalette moved(CommandEdit direction) const;
    // 結果の index 番目（範囲の中であること）。
    [[nodiscard]] const CommandChoice &row(std::size_t index) const;
    CommandLine input_;
    std::size_t selected_;
    // 開いたときの編集モード（面を開いている間は変わらない）。
    EditMode mode_;
    // 開くときに受け取った候補の列（ADR 0060 の決定 1）。入力が変わっても同じ列を指し、変わるのは
    // extended が後ろへ伸ばすときだけ（ADR 0062 の決定 16）。
    Entries entries_;
    std::shared_ptr<const Result> result_;
};
} // namespace nenenib::core
