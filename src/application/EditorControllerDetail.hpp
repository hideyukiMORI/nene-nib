#pragma once

#include "Composition.hpp"
#include "CompositionView.hpp"
#include "DisplayText.hpp"
#include "DocumentState.hpp"
#include "DocumentView.hpp"
#include "EditMode.hpp"
#include "EditorIntent.hpp"
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
// 合図（裏の仕事の完了と時計の合図: WorkCompleted・AutosaveTick・PreviewTick・
// LanguageServerTick・MapTimeout）。「この意図は合図か」を決める 1 本で、not_user_operation と
// 上下の移動の狙いの横位置を保つ側（keeps_wanted_x）がこれを呼ぶ（ADR 0062 の追補・#1296）。
// 合図を足すときはここに 1 行足す。
[[nodiscard]] bool is_signal(const EditorIntent &intent) noexcept;
// 使う人の操作ではない意図（合図・窓の寸法・外観・帯の hover）。keeps_tab_walk と
// begin_intent と `map` の待ちと、全面プレビューの振り分け（ADR 0155 の改訂 17）が共用する 1 本。
[[nodiscard]] bool not_user_operation(const EditorIntent &intent) noexcept;
// アクティブな文書が何も書いていない無題か（パス無し・本文 0 バイト・履歴 0）。F1 の本文の案内
// （guide_context の untouched_untitled・ADR 0078）と起動画面（start_screen_shown・ADR 0172 の
// 決定 1）が同じこの 1 本を読む。隠す外側の条件はそれぞれが持つ。
[[nodiscard]] bool untouched_untitled(const EditorState &state) noexcept;
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
