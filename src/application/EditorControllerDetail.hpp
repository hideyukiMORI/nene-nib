#pragma once

#include "Composition.hpp"
#include "CompositionView.hpp"
#include "DisplayText.hpp"
#include "DocumentState.hpp"
#include "DocumentView.hpp"
#include "EditMode.hpp"
#include "EditorState.hpp"
#include "FilePath.hpp"
#include "LineNumber.hpp"
#include "ParkedTab.hpp"
#include "ScrollExtent.hpp"
#include "SearchLine.hpp"
#include "TextBuffer.hpp"
#include "UnloadedDocument.hpp"
#include "VimPattern.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

// EditorController の群（ADR 0158）の 2 つ以上が呼ぶ補助と定数。controller の .cpp だけが include
// する。
namespace nenenib::application::detail
{
[[nodiscard]] core::ScrollExtent scroll_extent_for(core::EditMode mode) noexcept;
[[nodiscard]] std::string with_document_newlines(std::string_view utf8, std::string_view newline);
[[nodiscard]] const core::SearchLine *previewed_line(const EditorState &state);
// 検索の入力行の語を照合器 VimPattern の文字列にした値（`/` `?` は打ったまま・「検索: 」は
// literal_pattern で逃がした形・ADR 0174 の決定 4）。検索でない入力行（「行: 」）は値なし。
[[nodiscard]] std::optional<std::string> searched_text(const core::SearchLine &line);
// 入力中のパターン（incsearch の preview）。大小の旗は確定の検索と同じ設定の値（#1041）。
[[nodiscard]] std::optional<core::VimPattern> typed_pattern(const core::SearchLine &line,
                                                            core::VimPatternCase letter_case);
[[nodiscard]] const DocumentView &parked_view(const ParkedTab &tab);
[[nodiscard]] std::vector<DocumentView> tab_views(const EditorState &state,
                                                  const DocumentView &active);
[[nodiscard]] const std::optional<core::FilePath> &
parked_path(const std::shared_ptr<const DocumentState> &tab);
[[nodiscard]] std::optional<core::FilePath>
parked_path(const std::shared_ptr<const UnloadedDocument> &tab);
[[nodiscard]] core::LineNumber line_within(const core::TextBuffer &text, core::LineNumber line);
inline constexpr std::string_view clipped_mark = "…";
// 名前を 1 つ載せる 1 行。名前は prefix と suffix の残りに収まるよう末尾をコードポイントの境目で
// 落とす（題名と同じ「…」・DisplayText の 256 バイト）。named_notice と読み込み中の 1 行の共通。
[[nodiscard]] core::DisplayText named_line(std::string_view prefix, std::string name,
                                           std::string_view suffix);
[[nodiscard]] core::DisplayText named_notice(std::string_view prefix, std::string name,
                                             std::size_t others);
// 面と `:` の補完の面の見えている行数を決める窓の寸法（ADR 0150 の決定 1）。幅か高さが 0 なら
// 寸法が届く前で、行数は上限のまま。
using CommandWindow = std::tuple<std::int32_t, std::int32_t, std::uint32_t>;
[[nodiscard]] bool measured(CommandWindow window) noexcept;
[[nodiscard]] std::optional<CompositionView>
composition_view_of(const std::optional<core::Composition> &composition);
} // namespace nenenib::application::detail
