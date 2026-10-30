#pragma once

#include "CommandEdit.hpp"
#include "ExFailure.hpp"
#include "HistoryWalk.hpp"
#include "InputHistory.hpp"
#include "InputLinePrompt.hpp"
#include "InputText.hpp"
#include "Offset.hpp"
#include "VimSearchDirection.hpp"

#include <expected>
#include <optional>
#include <string_view>

namespace nenenib::core
{
// 検索の入力行（ADR 0032 の決定 1）。本文とは独立した、検証済みの 1 行。編集の規則は
// InputText の純関数で Ex の CommandLine と共用し、補完と ThemeCatalog は持たない。
// 通常モードの「検索: 」「行: 」も補完の無い同じ 1 行なので、プロンプトを持たせて
// この型で開く（ADR 0174 の決定 3・入力行は CommandInput の 1 本のまま）。生成経路は
// Vim の `/` `?` の opened と通常モードの opened_find / opened_replace_with / opened_go_to_line
// で、位置は UTF-8 の境界だけを通る（CPP-007）。
class SearchLine final
{
  public:
    [[nodiscard]] static SearchLine opened(VimSearchDirection direction);
    // 通常モードの「検索: 」。前回の語 preset を置いてキャレットを末尾に置く。preset が 1 行の
    // 入力にならなければ（長すぎる・改行を含む）空で開く。
    [[nodiscard]] static SearchLine opened_find(std::string_view preset);
    // 通常モードの置換の 2 段目「置換後: 」（ADR 0174 の決定 3）。前回の置換後の語 preset を置く
    // （入らなければ空・opened_find と同じ）。
    [[nodiscard]] static SearchLine opened_replace_with(std::string_view preset);
    // 通常モードの「行: 」（空で開く）。
    [[nodiscard]] static SearchLine opened_go_to_line();
    [[nodiscard]] std::string_view text() const noexcept;
    [[nodiscard]] Offset caret() const noexcept;
    // `/` 「検索: 」「置換後: 」「行: 」は forward、`?` は backward。
    [[nodiscard]] VimSearchDirection direction() const noexcept;
    // search_forward / search_backward / find / replace_with / go_to_line のどれか。
    [[nodiscard]] InputLinePrompt prompt() const noexcept;
    [[nodiscard]] std::expected<SearchLine, ExFailure> inserted(std::string_view text) const;
    [[nodiscard]] SearchLine edited(CommandEdit edit) const;
    // 履歴の上下（#725・CommandLine::recalled と同じ規則・履歴は検索の 1 本）。
    [[nodiscard]] SearchLine recalled(const InputHistory &history, CommandEdit direction) const;

  private:
    SearchLine(InputText input, InputLinePrompt prompt);

    InputText input_;
    InputLinePrompt prompt_;
    // 履歴の歩き（値があれば歩いている）。ほかの編集は作り直して歩きを終える。
    std::optional<HistoryWalk> history_walk_;
};
} // namespace nenenib::core
