#pragma once

#include "VimAction.hpp"
#include "VimBlockInsert.hpp"
#include "VimBlockRange.hpp"
#include "VimCharacterSearchInvocation.hpp"
#include "VimKeyTable.hpp"
#include "VimLastVisual.hpp"
#include "VimLayoutNeed.hpp"
#include "VimLineEndStop.hpp"
#include "VimMotionRange.hpp"
#include "VimNumberedRule.hpp"
#include "VimPutSide.hpp"
#include "VimScreenPosition.hpp"
#include "VimScrollDirection.hpp"
#include "VimStep.hpp"
#include "VimTextObject.hpp"
#include "VimTextObjectCancel.hpp"
#include "VimTextObjectOutcome.hpp"
#include "VimTextObjectSpan.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

// VimStep の群（ADR 0158 の決定 3）が翻訳単位をまたいで呼ぶ関数の宣言と、2 つ以上の群が使う定数。
// src/core/VimStep*.cpp だけが include する。
namespace nenenib::core::detail
{
[[nodiscard]] std::optional<VimMotion> laid_out_motion(const VimEditorView &view, VimAction action);
[[nodiscard]] LineNumber shifted_line(LineNumber line, std::size_t amount, std::size_t total,
                                      VimScrollDirection direction) noexcept;
[[nodiscard]] VimStep page_step(const VimState &state, const VimEditorView &view,
                                VimScrollDirection direction);
[[nodiscard]] bool page_stays(const VimEditorView &view, VimScrollDirection direction);
[[nodiscard]] VimStep scroll_action(const VimState &state, const VimEditorView &view,
                                    VimAction action);
[[nodiscard]] VimStep input_action(const VimState &state, const VimEditorView &view,
                                   VimAction action);
[[nodiscard]] VimStep tab_action(const VimState &state, const VimEditorView &view, VimAction action,
                                 VimStep refused);
[[nodiscard]] VimStep requested_layout(const VimState &state, const VimLayoutNeed &need);
[[nodiscard]] VimStep horizontal_action(const VimState &state, const VimEditorView &view,
                                        VimAction action);
[[nodiscard]] VimStep aligned_screen(const VimState &state, const VimEditorView &view,
                                     VimAction action);
[[nodiscard]] std::expected<VimEditorView, VimLayoutNeed>
display_view(const VimState &state, const VimEditorView &view, VimAction action);
[[nodiscard]] std::optional<VimStep>
requested_viewport(const VimState &state, const VimEditorView &view, VimAction action);
[[nodiscard]] bool counts_as_digit(const VimState &state, char32_t key) noexcept;
[[nodiscard]] VimStep counted(const VimState &state, char32_t key);
// `<Del>` が回数の最後の桁を消す鍵か（Vim の normal_cmd_get_count の `count0 != 0`・#1325）。
[[nodiscard]] bool drops_count_digit(const VimState &state, const VimKey &key) noexcept;
// 回数の最後の桁を消す（`12<Del>` は 1・`3<Del>` は回数なし）。
[[nodiscard]] VimStep count_digit_dropped(const VimState &state);
[[nodiscard]] VimStep window_started(const VimState &state);
// 1 鍵を待つ接頭辞の後ろの Esc は打ち消し、ほかの鍵は refused（#1369・VimStep.cpp）。
[[nodiscard]] VimStep escaped_or_refused(const VimState &state, const VimKey &key, VimStep refused);
[[nodiscard]] VimStep escaped_or_refused(const VimState &state, const VimKey &key);
[[nodiscard]] bool searches_forward(VimCharacterSearchKind kind) noexcept;
[[nodiscard]] VimCharacterSearchKind opposite(VimCharacterSearchKind kind) noexcept;
[[nodiscard]] std::optional<Offset>
character_search_position(const VimEditorView &view, const VimState &state,
                          VimCharacterSearch search, VimCharacterSearchInvocation invocation);
[[nodiscard]] Offset wrapped_forward(const TextBuffer &text, Offset caret, std::size_t count,
                                     VimLineEndStop stop);
[[nodiscard]] Offset wrapped_backward(const TextBuffer &text, Offset caret, std::size_t count,
                                      VimLineEndStop stop);
[[nodiscard]] LineNumber last_view_line(const VimEditorView &view) noexcept;
[[nodiscard]] LineNumber screen_line(const VimEditorView &view, VimScreenPosition position,
                                     std::size_t count) noexcept;
[[nodiscard]] LineNumber document_line(const TextBuffer &text, std::size_t count) noexcept;
[[nodiscard]] VimScreenPosition screen_position_of(VimMotion motion) noexcept;
[[nodiscard]] VimMotionLanding row_landing_in(const VimEditorView &view, VimMode mode);
[[nodiscard]] Offset column_landing(const TextBuffer &text, LineNumber line, std::size_t count,
                                    VimMode mode);
[[nodiscard]] std::optional<LineNumber> percent_line(const TextBuffer &text, std::size_t count);
[[nodiscard]] std::optional<Offset> paragraph_or_sentence(const TextBuffer &text, Offset caret,
                                                          VimMotion motion, std::size_t count);
[[nodiscard]] Offset mark_landing_of(const VimEditorView &view, VimMotion motion, VimMode mode);
[[nodiscard]] Offset moved_by(const VimEditorView &view, const VimState &state, VimMotion motion);
[[nodiscard]] VimMotionLanding landing_of(const VimEditorView &view, const VimState &state,
                                          VimMotion motion);
[[nodiscard]] VimWantedColumn wanted_after(const VimWantedColumn &wanted, VirtualColumn landed,
                                           VimMotion motion);
[[nodiscard]] VimWantedColumn wanted_for(const VimEditorView &view, const VimState &state,
                                         VimMotion motion);
[[nodiscard]] VimStep cancelled(const VimState &state);
[[nodiscard]] VimStep failed(VimStep step, VimRepeatFailure failure);
[[nodiscard]] VimStep moved_or_failed(VimStep step, VimMotion motion, Offset from,
                                      const VimMotionLanding &landing);
[[nodiscard]] VimStep context_marked(VimStep step, const VimEditorView &view);
[[nodiscard]] VimStep jumped_by(VimStep step, const VimEditorView &view, VimMotion motion);
[[nodiscard]] VimStep landed_step(const VimState &state, const VimEditorView &view,
                                  VimMotion motion);
[[nodiscard]] VimStep motion_step(const VimState &state, const VimEditorView &view,
                                  VimMotion motion);
[[nodiscard]] VimStep jump_list_action(const VimState &state, const VimEditorView &view,
                                       VimAction action);
[[nodiscard]] VimMotionRange characters_between(Offset first, Offset second);
[[nodiscard]] std::optional<VimMotionRange> lines_below(const TextBuffer &text, LineNumber line,
                                                        std::size_t steps);
[[nodiscard]] Offset inclusive_end(const TextBuffer &text, Offset at);
[[nodiscard]] VimMotionRange writing_empty(VimMotionRange range);
[[nodiscard]] std::optional<VimMotionRange> to_line_end(const TextBuffer &text, Offset caret,
                                                        std::size_t count, VimMotion motion);
[[nodiscard]] std::optional<VimMotionRange>
motion_range(const VimEditorView &view, VimMotion motion, std::size_t count, VimOperator operation);
[[nodiscard]] VimMotionRange exclusive_range(const TextBuffer &text, const VimMotionRange &range);
[[nodiscard]] VimMotionRange adjusted_for_exclusive(const TextBuffer &text,
                                                    const VimMotionRange &range, VimMotion motion);
[[nodiscard]] bool keeps_the_line_break(const TextBuffer &text, const VimMotionRange &range,
                                        VimMotion motion, VimOperator operation);
[[nodiscard]] VimMotionRange column_one_yank(const TextBuffer &text, const VimMotionRange &range);
[[nodiscard]] VimMotionRange whole_lines_for_delete(const TextBuffer &text,
                                                    const VimMotionRange &range);
// 書き手の結果（ADR 0051 の決定 6・追補（#848）の決定 C）。first は命令が完了した状態、second は
// `"+` `"*` へ書いたときと、`:set clipboard=unnamed` でレジスタを選ばずに無名へ書いたときだけ持つ
// 「OS へ出す本文」。呼び出し元が second を VimStep.clipboard へ写す。
using WrittenRegisters = std::pair<VimState, std::optional<VimRegister>>;
[[nodiscard]] std::expected<WrittenRegisters, VimRepeatFailure>
registers_written(const VimState &state, const std::optional<VimRegister> &value,
                  VimOperator operation, VimNumberedRule rule);
[[nodiscard]] VimStep with_clipboard(VimStep step, std::optional<VimRegister> clipboard);
[[nodiscard]] VimStep refused_register(const VimState &state);
[[nodiscard]] VimStep restored_at(VimStep step, Offset restore);
[[nodiscard]] Offset visual_restore(const VimState &state, const TextBuffer &text,
                                    const Selection &selection);
[[nodiscard]] VimStep removed_exactly(const VimState &state, const TextBuffer &text,
                                      const VimMotionRange &range);
[[nodiscard]] VimStep changed(const VimState &state, const TextBuffer &text,
                              const VimMotionRange &range);
[[nodiscard]] VimStep yanked(const VimState &state, const TextBuffer &text, Offset caret,
                             const VimMotionRange &range);
[[nodiscard]] std::string cased_text(std::string_view text, VimOperator operation);
[[nodiscard]] VimStep case_changed(const VimState &state, const TextBuffer &text,
                                   const VimMotionRange &range, VimOperator operation);
[[nodiscard]] std::optional<std::string> shifted_line(std::string_view line, VimOperator operation,
                                                      std::size_t amount);
// 字下げを動かす向き（shift_right / shift_left）と回数。回数は VISUAL の回数で、NORMAL は 1
// （Vim の op_shift の amount）。
using ShiftBy = std::pair<VimOperator, std::size_t>;
[[nodiscard]] VimStep shifted(const VimState &state, const TextBuffer &text,
                              const VimMotionRange &range, ShiftBy shift);
// `>` が lines 行（または range の覆う行）の字下げを作り直した後も文書が limit に収まるか
// （#1341）。1 行に足すのは多くとも回数 + 9 バイト（shiftwidth × 回数 / tabstop + tabstop + 1）で、
// `<` は本文を増やさない。NORMAL の `>>` は 1 段だけなので見ない。
[[nodiscard]] bool shift_fits(const TextBuffer &text, std::size_t lines, ShiftBy shift,
                              std::size_t limit) noexcept;
[[nodiscard]] bool range_shift_fits(const TextBuffer &text, const VimMotionRange &range,
                                    ShiftBy shift, std::size_t limit) noexcept;
[[nodiscard]] VimStep joined(const VimState &state, const VimEditorView &view, bool inserts_space);
// つなぐ最初の行と最後の行（J と gJ・NORMAL は回数から・VISUAL は選択の行から・#1417）。
using JoinedLines = std::pair<LineNumber, LineNumber>;
[[nodiscard]] VimStep lines_joined(const VimState &state, const TextBuffer &text, JoinedLines lines,
                                   bool inserts_space);
[[nodiscard]] VimStep swapped_characters(const VimState &state, const TextBuffer &text,
                                         Offset caret);
[[nodiscard]] VimOperator operation_of(const VimState &state) noexcept;
[[nodiscard]] VimStep performed(const VimState &state, const TextBuffer &text, Offset caret,
                                const VimMotionRange &range);
[[nodiscard]] Offset operator_start(Offset before, Offset landing) noexcept;
[[nodiscard]] VimState finished_input_wait(const VimState &state);
[[nodiscard]] VimStep completed_character_search(const VimState &state, const VimEditorView &view,
                                                 VimCharacterSearch search,
                                                 VimCharacterSearchInvocation invocation);
[[nodiscard]] VimStep started_character_search(const VimState &state, VimCharacterSearchKind kind);
[[nodiscard]] VimStep started_prefix(const VimState &state, VimPrefix prefix);
[[nodiscard]] VimStep text_object_action(const VimState &state, VimAction action);
[[nodiscard]] VimStep repeated_character_search(const VimState &state, const VimEditorView &view,
                                                VimAction action);
[[nodiscard]] VimStep operated(const VimState &state, const VimEditorView &view, VimMotion motion);
[[nodiscard]] VimStep pending_operator(const VimState &state, VimOperator operation);
[[nodiscard]] VimStep operated_on_lines(const VimState &state, const TextBuffer &text, Offset caret,
                                        VimOperator operation);
[[nodiscard]] VimStep operated_to_line_end(const VimState &state, const TextBuffer &text,
                                           Offset caret, VimOperator operation);
// 回数で本文を増やす編集（`p` `P` の回数・INSERT の回数の反復・`o` の回数・矩形の `p`・矩形の
// `I` `A` `c` `C` の写し・VISUAL と矩形の `>`）が、編集の後も文書の上限 limit（製品は
// maximum_document_bytes・ADR 0159 の決定 7・VimEditorView.document_limit）に収まる残りの
// バイト数と、piece を count 個並べた量が room に収まるか（#1341）。確保の前に掛け算をせずに
// 判定する。
[[nodiscard]] std::size_t count_room(const TextBuffer &text, std::size_t limit) noexcept;
[[nodiscard]] bool fits_repeated(std::size_t piece, std::size_t count, std::size_t room) noexcept;
// LF で数えた効果の本文を文書へ写した後のバイト数。controller が `\n` を文書の改行の形へ直すので、
// CRLF の文書では改行 1 つが 2 バイトになる（#1341）。
[[nodiscard]] std::size_t written_bytes(std::string_view body, const TextBuffer &text) noexcept;
// body を count 個並べた本文。文書へ写した後に limit を超えるなら確保せずに too_large。
[[nodiscard]] std::expected<std::string, VimRepeatFailure>
repeated(std::string_view body, std::size_t count, const TextBuffer &text, std::size_t limit);
[[nodiscard]] std::size_t last_code_point_size(std::string_view body);
[[nodiscard]] VimInsertAt put_characters(const TextBuffer &text, Offset caret, std::string body,
                                         VimPutSide side);
[[nodiscard]] VimInsertAt put_lines(const TextBuffer &text, Offset caret, std::string body,
                                    VimPutSide side);
[[nodiscard]] std::optional<VimRegisterSelection> register_selection_of(char32_t name);
[[nodiscard]] VimStep selected_register(const VimState &state, const VimKey &key);
[[nodiscard]] VimBlockRange block_of(const VimState &state, const VimEditorView &view);
[[nodiscard]] VimStep block_operated(const VimState &state, const VimEditorView &view,
                                     VimOperator operation);
// 矩形の `D`。選んだ矩形を `$` で取り直して消す（`C` と同じ取り直し・#1417）。
[[nodiscard]] VimStep block_removed_to_line_end(const VimState &state, const VimEditorView &view);
[[nodiscard]] VimStep replaced_block(const VimState &state, const VimEditorView &view,
                                     char32_t target);
[[nodiscard]] VimStep cased_block(const VimState &state, const VimEditorView &view,
                                  VimOperator operation);
// 矩形の `I` `A`（入口）と `c` `C`（消してから入る）と、Esc で 1 行目の本文を写す 1 行の形と、
// 矩形の `>` `<`（ADR 0035 の追補 #1042）。
[[nodiscard]] VimStep block_inserted(const VimState &state, const VimEditorView &view,
                                     VimAction action);
[[nodiscard]] VimStep block_changed(const VimState &state, const VimEditorView &view,
                                    VimColumnWish wish);
[[nodiscard]] std::optional<VimBlockEdit> block_insert_edit(const TextBuffer &text, LineNumber line,
                                                            const VimBlockInsert &insert,
                                                            std::string_view body);
[[nodiscard]] std::string block_copied_body(const VimBlockInsert &insert, std::string body);
[[nodiscard]] VimStep shifted_block(const VimState &state, const VimEditorView &view,
                                    ShiftBy shift);
[[nodiscard]] VimRegister register_read(const VimState &state,
                                        const std::optional<VimRegisterSelection> &chosen);
[[nodiscard]] VimStep put_step(const VimState &state, const VimEditorView &view, VimPutSide side);
[[nodiscard]] VimStep started_replacement(const VimState &state);
[[nodiscard]] VimStep replacement_key(const VimState &state, const VimEditorView &view, VimKey key);
[[nodiscard]] VimMode rested_mode(VimMode mode);
[[nodiscard]] VimStep searched_key(const VimState &original, const VimEditorView &view,
                                   const VimSearchPattern &key);
[[nodiscard]] VimStep search_action(const VimState &state, const VimEditorView &view,
                                    VimAction action);
[[nodiscard]] VimState visual_resting(const VimState &state, VimMode mode);
[[nodiscard]] VimLastVisual visual_left(const VimState &state, const VimEditorView &view);
[[nodiscard]] std::optional<VimStep> reselected_visual(const VimState &state,
                                                       const VimEditorView &view);
[[nodiscard]] VimStep visual_acted(const VimState &state, const VimEditorView &input,
                                   VimAction action);
[[nodiscard]] VimStep visual_character(const VimState &state, const VimEditorView &view,
                                       VimCharacter key);
// 数の増減（VimStepNumbers.cpp・ADR 0015 の決定 14）。
[[nodiscard]] VimStep numbered(const VimState &state, const VimEditorView &view, VimAction action);
[[nodiscard]] VimStep visual_numbered(const VimState &state, const VimEditorView &view,
                                      VimAction action);
[[nodiscard]] VimStep visual_special(const VimState &state, const VimEditorView &view,
                                     VimSpecialKey key);
[[nodiscard]] VimStep insert_special(const VimState &state, const VimEditorView &view,
                                     VimSpecialKey key);
[[nodiscard]] VimStep insert_character(const VimState &state, VimCharacter key);
[[nodiscard]] VimStep insert_register_step(const VimState &state, const VimEditorView &view,
                                           const VimKey &key);
[[nodiscard]] bool restarts_insert(VimKey key) noexcept;
[[nodiscard]] bool erased_nothing(const VimKey &key, const VimEffect &effect) noexcept;
[[nodiscard]] bool moved_nothing(const VimKey &key, const VimEffect &effect) noexcept;
[[nodiscard]] VimState insert_started(const VimState &state, const VimEditorView &view,
                                      const VimKey &key);
[[nodiscard]] VimStep insert_start_kept(const VimState &before, VimStep step, const VimKey &key);
[[nodiscard]] VimStep completed_text_object(const VimState &state, const VimEditorView &view,
                                            const VimTextObjectSpan &span);
[[nodiscard]] VimStep completed_text_object(const VimState &state, const VimEditorView &view,
                                            const VimTextObjectCancel &cancel);
[[nodiscard]] VimTextObjectOutcome word_writing_empty(VimTextObjectOutcome outcome,
                                                      VimTextObject object);
[[nodiscard]] std::optional<char32_t> macro_name_of(const VimKey &key) noexcept;
[[nodiscard]] VimStep started_macro(const VimState &state, const VimKey &key);
[[nodiscard]] VimRegister appended_recording(const VimRegister &old, std::string_view text);
[[nodiscard]] VimStep macro_record_action(const VimState &state, const VimEditorView &view);
[[nodiscard]] VimStep replayed_macro(const VimState &state, const VimKey &key);
[[nodiscard]] VimStep repeated_change(const VimState &state);
[[nodiscard]] VimStep vim_recorded(const VimState &before, const VimEditorView &view, VimStep step,
                                   VimKey key);
[[nodiscard]] VimStep macro_recorded(const VimState &before, const VimEditorView &view,
                                     VimStep step, VimKey key);
inline constexpr std::size_t single_step = 1;
inline constexpr std::size_t decimal_base = 10;
inline constexpr char32_t line_feed = U'\n';
inline constexpr char32_t carriage_return_character = 0x0D;
[[nodiscard]] std::size_t count_of(const std::optional<VimCount> &count) noexcept;
[[nodiscard]] std::optional<VimCount> combined_count(const VimState &state) noexcept;
[[nodiscard]] std::size_t resolved_count(const VimState &state) noexcept;
[[nodiscard]] std::size_t resolved_motion_count(const VimState &state, const TextBuffer &text,
                                                VimMotion motion) noexcept;
[[nodiscard]] LineNumber line_of(const TextBuffer &text, Offset caret);
[[nodiscard]] Offset forward_characters(const TextBuffer &text, Offset caret, std::size_t count);
[[nodiscard]] Offset backward_characters(const TextBuffer &text, Offset caret, std::size_t count);
[[nodiscard]] VimWantedColumn wanted_column_of(const TextBuffer &text, const VimState &state,
                                               Offset caret);
[[nodiscard]] VirtualColumn landed_column(const TextBuffer &text, VimMode mode, Offset anchor,
                                          Offset moved);
[[nodiscard]] Offset rested_in(const TextBuffer &text, Offset caret, VimMode mode);
[[nodiscard]] Offset caret_on_line(const TextBuffer &text, const VimWantedColumn &wanted,
                                   LineNumber line, VimMode mode);
[[nodiscard]] LineNumber line_below(const TextBuffer &text, LineNumber line, std::size_t count);
[[nodiscard]] LineNumber line_above(LineNumber line, std::size_t count);
} // namespace nenenib::core::detail
