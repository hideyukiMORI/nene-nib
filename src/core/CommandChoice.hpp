#pragma once

#include "CommandChoiceKind.hpp"
#include "CommandLine.hpp"
#include "DisplayText.hpp"
#include "PaletteOrigin.hpp"
#include "PaletteScope.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
struct CommandChoice
{
    DisplayText label;
    std::string command;
    CommandChoiceKind kind;
    // 題名の後ろに muted で添える補足（ADR 0057 の決定 7）。タブの候補ではファイルのあるフォルダ
    // （無題は無し）、Ex の候補では無し。
    std::optional<DisplayText> detail = std::nullopt;
    // 候補の出どころの印（ADR 0060 の決定 3）。Ex のコマンドの候補は印なし。
    std::optional<PaletteOrigin> origin = std::nullopt;
};

[[nodiscard]] std::vector<CommandChoice> palette_choices(const CommandLine &input);
// Ctrl+P の候補の列の絞り込みと順（ADR 0060 の決定 4）。scope に入る候補だけを残し（files は印の
// ある候補の全部・commands はこの関数の外なので空）、query が空なら列の順、あれば名前に
// match_score（大文字と小文字を区別しない部分列）、名前に当たらなければ「場所＋名前」に当てて
// 固定の罰点を足す。点の小さい順で、同点は列の順。結果は entries の中の位置の列で、候補の写しを
// 作らない（ADR 0062 の決定 1）。
[[nodiscard]] std::vector<std::size_t> listed_positions(const std::vector<CommandChoice> &entries,
                                                        PaletteScope scope, std::string_view query);
} // namespace nenenib::core
