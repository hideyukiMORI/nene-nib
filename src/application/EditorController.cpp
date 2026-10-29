#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "CaretShape.hpp"
#include "CommandChoice.hpp"
#include "CommandInput.hpp"
#include "CommandPalette.hpp"
#include "DisplayInquiry.hpp"
#include "DisplayText.hpp"
#include "ExResult.hpp"
#include "FolderBatch.hpp"
#include "FolderProgress.hpp"
#include "GiantLine.hpp"
#include "GridLineRows.hpp"
#include "GridLineSource.hpp"
#include "ImeStance.hpp"
#include "LineSpanEvaluation.hpp"
#include "ModeLabel.hpp"
#include "PaneFrame.hpp"
#include "PaneView.hpp"
#include "PreviewFrame.hpp"
#include "PreviewView.hpp"
#include "SearchLine.hpp"
#include "Selection.hpp"
#include "SelectionSpan.hpp"
#include "StatusItems.hpp"
#include "SyntaxLines.hpp"
#include "TabTitle.hpp"
#include "TextCaret.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimBlockRange.hpp"
#include "VimMode.hpp"
#include "VimSearch.hpp"
#include "VimVisualRange.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
using detail::composition_view_of;
using detail::previewed_line;
using detail::tab_views;
using detail::typed_pattern;

namespace
{
// 読めない理由（unavailable / unreadable）は区別せず既定の dark を選ぶ（ADR 0007）。
[[nodiscard]] core::Appearance appearance_or_dark(const AppearancePort &port)
{
    const auto current = port.current();
    if (!current)
    {
        return core::Appearance::dark;
    }
    return current.value();
}

// INSERT だけがバー。NORMAL と VISUAL はブロック（採用案 第 1 節・D15 / ADR 0018 の決定 1）。
[[nodiscard]] core::CaretShape caret_shape_for(core::EditMode mode, core::VimMode vim) noexcept
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return core::CaretShape::bar;
    case core::EditMode::vim:
        break;
    }
    switch (vim)
    {
    case core::VimMode::normal:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        return core::CaretShape::block;
    case core::VimMode::insert:
        return core::CaretShape::bar;
    }
    std::unreachable();
}

// 描く範囲。矩形のあいだだけ行ごとの範囲へ差し替わる（ADR 0035 の決定 8）。短い行の右側
// （本文の無い所）は塗らない＝ Vim の既定と同じ。
[[nodiscard]] core::OffsetRange block_row(const std::optional<core::VimBlockRange> &block,
                                          core::LineNumber line, const core::OffsetRange &range)
{
    if (!block.has_value())
    {
        return range;
    }
    const core::VimBlockRange &rows = block.value();
    if (line.value < rows.first.value || line.value >= rows.first.value + rows.lines.size())
    {
        return core::OffsetRange{core::Offset{0}, core::Offset{0}};
    }
    return rows.lines.at(line.value - rows.first.value).range;
}

// incsearch の preview が着いた当たりの位置。入力中でないか当たりが無ければ absent（ADR 0041 の
// 決定 4）。
[[nodiscard]] std::optional<core::Offset> previewed_offset(const EditorState &state)
{
    const auto &preview = state.search_preview();
    if (!preview.has_value())
    {
        return std::nullopt;
    }
    const auto &match = preview.value().match;
    if (!match.has_value())
    {
        return std::nullopt;
    }
    return state.text().offset_of(match.value());
}

// 帯の順のタブの表示値（ADR 0056 の決定 7）。脇の束は置いたときの値を並べるだけで、
// 題名を作り直さない。アクティブの分は document と同じ値を差し込む。
// ステータスバーの右の 4 項目（ADR 0154 の決定 6・言語は右端）。
[[nodiscard]] std::array<core::DisplayText, core::status_item_count>
status_items_of(const EditorState &state, core::TextPosition caret, core::TextEncoding encoding)
{
    return core::status_items_for(caret, encoding, state.line_ending(), state.language());
}
// 描けない失敗だけを残す（frame の欄・ADR 0133 の追補 2026-10-18）。
[[nodiscard]] std::optional<TextLayoutFailure>
drawing_failure(std::optional<TextLayoutFailure> failure) noexcept
{
    if (failure.has_value() && blocks_drawing(failure.value()))
    {
        return failure;
    }
    return std::nullopt;
}

// 使う人の操作ではない意図（ADR 0176 の決定 4・ADR 0062 の決定 7・ADR 0137 の決定 1・ADR 0155 の
// 決定 8・ADR 0160 の決定 6）。裏の仕事と合図（WorkCompleted・AutosaveTick・PreviewTick・
// LanguageServerTick・MapTimeout）と、窓の
// 寸法・外観・帯の hover。keeps_tab_walk と begin_intent と `map` の待ちを保つ側が共用する。
[[nodiscard]] bool not_user_operation(const EditorIntent &intent) noexcept
{
    return std::holds_alternative<WorkCompleted>(intent) ||
           std::holds_alternative<AutosaveTick>(intent) ||
           std::holds_alternative<PreviewTick>(intent) ||
           std::holds_alternative<LanguageServerTick>(intent) ||
           std::holds_alternative<MapTimeout>(intent) ||
           std::holds_alternative<VisibleLines>(intent) ||
           std::holds_alternative<TitleBarWidth>(intent) ||
           std::holds_alternative<RefreshAppearance>(intent) ||
           std::holds_alternative<PointTitleBar>(intent);
}

// Ctrl+Tab の歩きを続けたまま受け取れる意図（ADR 0058 の決定 4）。歩きの 1 歩と確定、それに窓の
// 寸法・外観・帯の上のマウスとホイールのように文書もアクティブのタブも動かさない意図。これ以外の
// 意図は写す前に歩きを確定する（Ctrl を押したまま帯の上でマウスが動いても、歩きは切れない）。
// 裏の仕事の合図（WorkCompleted）も使う人の操作ではないので歩きを続ける（ADR 0062 の決定 7）。
// 自動退避の合図（AutosaveTick）も同じ理由で歩きを続ける（ADR 0137 の決定 1）。起動の問いの答え
// （RestoreAutosave / DiscardAutosave）は使う人の操作なので、ここに書かず歩きを確定する（決定 6）。
// プレビューの間引きの合図（PreviewTick）も使う人の操作ではないので歩きを続ける（ADR 0155 の
// 決定 8）。開く・閉じる（TogglePreview）とチェックボックスの切り替え（TogglePreviewTask・ADR 0155
// の追補 24）は使う人の操作なので、ここにも begin_intent にも書かない。
// エクスプローラーの意図（ToggleExplorer・ExplorerKey・PointExplorer・ScrollExplorer・
// ExplorerRootChosen）も使う人の操作なので、どちらにも書かない（ADR 0162 の決定 4）。
// 言語サーバーの古さの合図（LanguageServerTick）も使う人の操作ではないので歩きを続ける（ADR 0160
// の決定 6）。
[[nodiscard]] bool keeps_tab_walk(const EditorIntent &intent) noexcept
{
    return not_user_operation(intent) || std::holds_alternative<WalkRecentTab>(intent) ||
           std::holds_alternative<SettleRecentTab>(intent) ||
           std::holds_alternative<ScrollTabs>(intent);
}

// Vim の `map` の待ちで積んだ鍵を、写す前に流さずに保つ意図（ADR 0176 の決定 4）。打った鍵
// （VimKeyPress は入口の表が積み・流す）と使う人の操作ではない意図だけ。使う人の操作（IME の
// 確定・マウス・Ctrl+Tab の歩きと確定・帯のホイール・モードを変える意図…）は、受ける前に積んだ
// 鍵を順に流す。
[[nodiscard]] bool keeps_key_map_pending(const EditorIntent &intent) noexcept
{
    return std::holds_alternative<VimKeyPress>(intent) || not_user_operation(intent);
}

// 上下に動くときに狙う横位置を保つ意図。上下の移動そのものと、30 秒ごとに必ず来る自動退避の
// 合図（使う人の操作ではない・ADR 0137 の決定 1）とプレビューの間引きの合図（ADR 0155 の追補・
// #813）。もう 1 つの枠だけを送るホイールも操作中の枠の
// キャレットに触れないので保つ（ADR 0148 の決定 11）。これ以外の意図は写す前に狙いを捨てる。
// 枠の意図（SplitPane / SwitchPane / ClosePane / ScrollPane）は使う人の操作なので、
// keeps_tab_walk と begin_intent の表には載せない（歩きを終え、告知を消す）。
[[nodiscard]] bool keeps_wanted_x(const EditorIntent &intent) noexcept
{
    return std::holds_alternative<MoveCaret>(intent) ||
           std::holds_alternative<AutosaveTick>(intent) ||
           std::holds_alternative<PreviewTick>(intent) ||
           std::holds_alternative<LanguageServerTick>(intent) ||
           std::holds_alternative<ScrollPane>(intent);
}

// 格子の照合の窓の余裕（#556）。一致の長さの上限が決まらない（`*` の繰り返し）検索は、この余裕の
// 外へ延びる一致が窓の境目で切れる（ADR 0140 の限界）。数は text の表示の窓と同じだが、ここでは
// バイト（text の表示の窓はコードポイント）。
constexpr std::size_t grid_unbounded_margin = core::giant_line_half_width;
// 窓の前後に読み足すバイト数。窓の端を code point の境目へ寄せる分と、`^` `\<` `\>` が見る
// 隣の 1 code point の分（UTF-8 で 4 バイトずつ）。
constexpr std::size_t grid_context_bytes = 8;

// 見えている範囲 visible に余裕 margin を前後に足し、行の内容 [line.begin, line.end) に収めた窓。
[[nodiscard]] core::OffsetRange
grid_search_window(core::OffsetRange line, core::OffsetRange visible, std::size_t margin) noexcept
{
    const auto begin =
        visible.begin.value - std::min(visible.begin.value - line.begin.value, margin);
    const auto end = visible.end.value + std::min(line.end.value - visible.end.value, margin);
    return core::OffsetRange{core::Offset{begin}, core::Offset{end}};
}

// bytes の at を code point の始まりへ前に寄せる / 後ろへ寄せる。
[[nodiscard]] std::size_t boundary_before(std::string_view bytes, std::size_t at) noexcept
{
    while (at > 0 && !core::is_boundary(bytes, core::Offset{at}))
    {
        --at;
    }
    return at;
}

[[nodiscard]] std::size_t boundary_after(std::string_view bytes, std::size_t at) noexcept
{
    while (at < bytes.size() && !core::is_boundary(bytes, core::Offset{at}))
    {
        ++at;
    }
    return at;
}

// 窓 window（読んだ bytes の中の位置）の当たりを、窓の前後の 1 code point の文脈を付けて照合し、
// 本文の位置 read_begin から数えた本文の位置で返す。文脈は読んだ範囲の中だけで、読んだ範囲の端が
// 行の端なら `^` `$` はそこで当たる。
[[nodiscard]] std::vector<core::OffsetRange> grid_window_matches(std::string_view bytes,
                                                                 core::OffsetRange window,
                                                                 std::size_t read_begin,
                                                                 const core::VimPattern &pattern)
{
    const auto from = boundary_before(bytes, window.begin.value);
    const auto to = boundary_after(bytes, window.end.value);
    const auto first = from == 0 ? 0 : core::previous_code_point(bytes, core::Offset{from}).value;
    const auto last =
        to == bytes.size() ? to : core::next_code_point(bytes, core::Offset{to}).value;
    auto matches = core::vim_window_matches(
        bytes.substr(first, last - first),
        core::OffsetRange{core::Offset{from - first}, core::Offset{to - first}}, pattern);
    for (auto &match : matches)
    {
        match = core::OffsetRange{core::Offset{read_begin + first + match.begin.value},
                                  core::Offset{read_begin + first + match.end.value}};
    }
    return matches;
}

// text の表示の 1 行 shown の当たりを本文の位置で返す。巨大な行の窓（ADR 0139）は格子と同じく
// 窓の前後の 1 code point を本文から文脈に読み足し、grid_window_matches で照合する（#556・
// 照合の経路は 1 本）。窓の無い行は窓 = 行の全体なので、文脈を読み足さず shown をそのまま使う。
[[nodiscard]] std::vector<core::OffsetRange>
shown_matches(const core::TextBuffer &text, const LineView &shown, const core::VimPattern &pattern)
{
    const core::OffsetRange content{text.line_start(shown.number), text.line_end(shown.number)};
    const core::Offset begin{content.begin.value + shown.window.byte};
    const core::OffsetRange window{begin, core::Offset{begin.value + shown.text.size()}};
    const auto read = grid_search_window(content, window, grid_context_bytes);
    if (read == window)
    {
        return grid_window_matches(
            shown.text, core::OffsetRange{core::Offset{0}, core::Offset{shown.text.size()}},
            begin.value, pattern);
    }
    const auto bytes = text.text_range(read.begin, read.end);
    return grid_window_matches(
        bytes,
        core::OffsetRange{core::Offset{window.begin.value - read.begin.value},
                          core::Offset{window.end.value - read.begin.value}},
        read.begin.value, pattern);
}
} // namespace

EditorController::EditorController(EditorPorts ports, const std::vector<OpenDocument> &initial)
    : ports_(ports),
      state_(EditorState::create(appearance_or_dark(ports.appearance), core::EditMode::ordinary))
{
    const auto inventory = ports_.themes.read();
    state_ = state_.with_themes(inventory.catalog);
    const auto loaded = ports_.settings.read(state_.themes());
    if (!loaded)
    {
        state_ = state_.with_settings_failure(loaded.error());
    }
    else
    {
        state_ = state_.with_settings(loaded.value().value_or(state_.settings()));
    }
    // 面が出ている設定ならルートの木を作って読み始める（ADR 0162 の決定 3・5）。
    sync_explorer();
    refresh_key_map();
    // 起動引数のファイルは順にタブで開き、最後に開けたものがアクティブになる（ADR 0056 の決定
    // 13）。失敗は 1 意図だけの値なので、後の引数が消さないよう最後の失敗を控えて載せ直す。
    std::optional<FileFailure> failure;
    for (const OpenDocument &document : initial)
    {
        static_cast<void>(apply(document));
        failure = state_.last_failure().has_value() ? state_.last_failure() : failure;
    }
    state_ = state_.with_failure(failure);
    // ファイルの引数が無いときだけ前回のタブを戻す（ADR 0059 の決定 6・D24）。
    if (initial.empty())
    {
        restore_session();
    }
    // 起動の問いは最後にアクティブになった文書だけ（ADR 0140 の決定 2）。途中で開いた文書の印は
    // 次の引数の意図が消すので、判定は起動の終わりの 1 回だけ。ほかの文書は見るときに判定する。
    starting_ = false;
    ask_display();
    // 前回のタブを戻した後のアクティブな文書で言語サーバーを合わせる（ADR 0160 の決定 6・オフなら
    // port を呼ばない）。
    reconcile_language_server();
    // 初期ファイルも通常の意図を通す。その後で起動時の診断を載せ、最初の描画まで保持する。
    // 前回のタブの知らせがあればそれを残す（ステータスバーは 1 行）。
    if (!state_.command_message().has_value())
    {
        state_ = state_.with_command_message(inventory.notice);
    }
}

void EditorController::accept(const AdjustFontSize &intent)
{
    auto settings = state_.settings();
    settings.font_size =
        core::adjusted_font_size(settings.font_size, intent.adjustment, intent.steps);
    (void)persist_settings(std::move(settings));
}

bool EditorController::persist_settings(core::EditorSettings settings)
{
    if (core::same_settings(settings, state_.settings()) && !state_.settings_failure().has_value())
    {
        return true;
    }
    const auto saved = ports_.settings.write(settings);
    if (!saved)
    {
        state_ = state_.with_settings_failure(saved.error());
        return false;
    }
    const bool reshaped = core::affects_line_height(state_.settings(), settings);
    state_ = state_.with_settings(std::move(settings)).with_settings_failure(std::nullopt);
    // 面の表示とルートは設定の値（ADR 0162 の決定 3）。Ex・面の `:`・意図のどれから書いても同じ 1
    // 本。
    sync_explorer();
    refresh_key_map();
    if (view_geometry_.width <= 0)
    {
        return true;
    }
    if (reshaped)
    {
        // 古い行高のまま組まない。clamp も頼むだけにし、窓が送る新しい幾何の後に解決する
        // （ADR 0128 の決定 8）。
        pending_clamp_ = true;
        follow_caret();
        return true;
    }
    clamp_scroll();
    follow_caret();
    return true;
}

EditorDelivery EditorController::apply(const EditorIntent &intent)
{
    layout_failure_.reset();
    repaint_ = true;
    if (!keeps_wanted_x(intent))
    {
        wanted_x_.reset();
    }
    settle_tab_walk_before(keeps_tab_walk(intent));
    // 窓の寸法と帯の上のマウスは文書に触れないので、Vim の告知を消さない（ADR 0056 の決定 3）。
    // Ctrl を離したときの使った順の確定も同じ（ADR 0058 の決定 3）。窓が閉じていくときの一覧の
    // 書き出しも同じ（ADR 0059 の決定 3）。裏の仕事の合図も同じ（ADR 0062 の決定 7）。
    // 自動退避の合図とプレビューの間引きの合図も使う人の操作ではないので同じ（ADR 0137 の決定 1・
    // ADR 0155 の決定 8）。言語サーバーの古さの合図も同じ（ADR 0160 の決定 6）。起動の問いの
    // 答えは使う人の操作なので、ほかの意図と同じに 1 意図の知らせを消してから写す（決定 6）。
    begin_intent(not_user_operation(intent) || std::holds_alternative<CancelComposition>(intent) ||
                 std::holds_alternative<ScrollTabs>(intent) ||
                 std::holds_alternative<SettleRecentTab>(intent) ||
                 std::holds_alternative<EndSession>(intent));
    // `map` の待ちの鍵は、待ちを保つ意図のほかは受ける前に流す（ADR 0176 の決定 4）。
    // 窓が閉じていく EndSession では流さずに捨てる（保存済みの文書を未保存にしない・#1096 の
    // R1-2）。
    map_timer_.reset();
    if (std::holds_alternative<EndSession>(intent))
    {
        discard_key_map_pending();
    }
    else if (!keeps_key_map_pending(intent))
    {
        flush_key_map_pending();
    }
    // 写し先が足りなければここでコンパイルが落ちる＝意図が増えたことに機械が気づく（CPP-002）。
    // SelectDisplay は使う人の操作の側（歩きを終え、告知を消す）なので上の 2 つの表に載せない。
    // SelectLanguage も同じ（ADR 0154 の決定 4）。SelectSyntax も同じ（ADR 0156 の追補）。
    // 面のホイール ScrollPaletteList も使う人の操作の側なので載せない（ADR 0150 の決定 2）。
    std::visit([this](const auto &value) { this->accept(value); }, intent);
    // 閉じたタブの読みの取消と、歩き・入力行・面の終わりで留めた読みの採用（ADR 0159 の追補
    // 1・2）。
    follow_reads();
    fit_command_rows();
    ask_display();
    reconcile_language_server();
    auto delivered = delivery();
    delivered.repaint = repaint_;
    delivered.map_timer = map_timer_;
    // 契約の破れは配達に 1 回だけ載せ、ここで消す（ADR 0133 の追補 2026-10-18）。次の apply の
    // 消去に頼らない。layout を作れない失敗は作り直しの apply で消えるまで frame に載る。
    layout_failure_ = drawing_failure(layout_failure_);
    return delivered;
}

EditorFrame EditorController::apply_frame(const EditorIntent &intent)
{
    static_cast<void>(apply(intent));
    return frame();
}

bool EditorController::is_composing() const noexcept
{
    return state_.composition().has_value();
}

std::vector<DocumentView> EditorController::documents() const
{
    return tab_views(state_, active_document_view());
}

// 歩いている間に歩きを続けない意図や Vim の鍵が来たら、写す前に SettleRecentTab と同じ確定を
// 通す（ADR 0058 の決定 4）。controller の入口はどれもここを通るので、窓を経ない経路でも確定が
// 漏れない。1 打鍵ごとに通るので、歩いていないときは欄を 1 つ読むだけで抜ける。
void EditorController::settle_tab_walk_before(bool keeps_walk)
{
    if (state_.tab_walking() && !keeps_walk)
    {
        accept(SettleRecentTab{});
    }
}

void EditorController::begin_intent(bool keeps_message)
{
    // ファイルの失敗は 1 つの意図のあいだだけ表示値に載る（ADR 0010 の決定 9）。
    // 最後のタブを閉じる印も同じく 1 意図だけ（ADR 0056 の決定 6）。
    // 一覧で頼まれた操作も同じく 1 意図だけ（ADR 0078 の決定 8）。
    // 格子表示を聞く印も同じく 1 意図だけ（ADR 0140 の決定 2）。
    if (state_.last_failure().has_value() || state_.closing() ||
        state_.close_request().has_value() || state_.operation_request().has_value() ||
        state_.display_question())
    {
        state_ = state_.with_intent_cleared();
    }
    unreached_tabs_ = 0;
    if (state_.command_message().has_value() && !keeps_message)
    {
        state_ = state_.with_command_message(std::nullopt);
    }
}

// 打ち切りは 1 回の知らせではなく今の面の状態（ADR 0062 の決定 17）。状態の知らせが出る意図では
// その 1 回だけそちらが出て、次の意図で打ち切りの 1 行へ戻る。面を閉じると出ない。
std::optional<core::DisplayText> EditorController::command_message() const
{
    if (state_.command_message().has_value())
    {
        return state_.command_message();
    }
    // 読み込み中の 1 行も今の状態の知らせで、要求が消えれば出ない（ADR 0159 の決定 4）。
    auto reading = reading_message();
    if (reading.has_value())
    {
        return reading;
    }
    if (!command_palette_active() || !palette_folder_.truncated)
    {
        return std::nullopt;
    }
    return core::DisplayText::parse("同じフォルダは " + std::to_string(palette_folder_.received) +
                                    " 件まで。残りは一覧に出ません")
        .value();
}

// 描く選択と Ctrl+C / Ctrl+X が覆う本文。VISUAL のあいだだけ Vim の規則で決まり、
// 表示と操作が同じ 1 本の範囲を使う（ADR 0018 の決定 5）。
core::OffsetRange EditorController::highlighted_range() const
{
    switch (state_.mode())
    {
    case core::EditMode::ordinary:
        return core::selection_range(state_.selection());
    case core::EditMode::vim:
        break;
    }
    return core::vim_visual_range(state_.text(), state_.selection(), state_.vim().mode).range;
}

// 矩形は 1 つの範囲では表せないので、モードで先に分かれる（ADR 0035 の決定 2）。
std::optional<core::VimBlockRange> EditorController::block_selection() const
{
    switch (state_.mode())
    {
    case core::EditMode::ordinary:
        return std::nullopt;
    case core::EditMode::vim:
        break;
    }
    switch (state_.vim().mode)
    {
    case core::VimMode::normal:
    case core::VimMode::insert:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
        return std::nullopt;
    case core::VimMode::visual_block:
        break;
    }
    return core::vim_block_range_for(state_.text(), state_.selection(), state_.vim().wanted_column);
}

const core::VimState &EditorController::vim_state() const noexcept
{
    return state_.vim();
}

const SplitState &EditorController::split() const noexcept
{
    return state_.split();
}

void EditorController::accept(const RefreshAppearance &)
{
    state_ = state_.with_appearance(appearance_or_dark(ports_.appearance));
}

// 裏の仕事の合図（ADR 0062 の決定 7・15・17）。入口は歩きも知らせも残す。collect は面が開いて
// いなくても 1 回呼んでたまりを空にする。使うのは面が開いていて今の券の batch だけ。
// 届いた分は 1 回の extended で後ろへ足す（入力と選択は core が保つ）。
// 打ち切りは今の面の欄に覚えるだけで、知らせは frame を作る command_message() が出す。
// 入力・知らせ・ほかの状態には触れない。自動退避の失敗もここで引く（ADR 0137 の決定 4）。
void EditorController::accept(const WorkCompleted &)
{
    collect_autosave();
    // 開く道の読み（ADR 0159 の決定 9）。今の券の結果だけを採る。
    collect_reads();
    // エクスプローラーの分は木へ写し、port が空いたら次のフォルダを頼む（ADR 0162 の決定 5）。
    const auto batches = receive_folders();
    request_explorer_folder();
    // 退避 → フォルダ → Markdown の順（ADR 0155 の決定 6）。プレビューが閉じていれば呼ばない。
    collect_preview();
    // 言語サーバー（ADR 0160 の決定 1）。子が居なければ呼ばない。
    collect_language_server();
    const auto &input = state_.command_input();
    const auto *palette =
        input.has_value() ? std::get_if<core::CommandPalette>(&input.value()) : nullptr;
    if (palette == nullptr)
    {
        return;
    }
    std::vector<core::CommandChoice> more;
    for (const FolderBatch &batch : batches)
    {
        if (batch.ticket != palette_folder_.ticket)
        {
            continue;
        }
        palette_folder_.received += batch.files.size();
        append_folder_choices(batch.files, more);
        switch (batch.progress)
        {
        case FolderProgress::more:
        case FolderProgress::complete:
        case FolderProgress::failed:
            break;
        case FolderProgress::truncated:
            palette_folder_.truncated = true;
            break;
        }
    }
    if (!more.empty())
    {
        auto extended = palette->extended(std::move(more));
        state_ = state_.with_command_input(std::move(extended));
    }
}

bool EditorController::tab_walking() const noexcept
{
    return state_.tab_walking();
}

const core::TextBuffer &EditorController::text() const noexcept
{
    return state_.text();
}

core::TitleBarInput EditorController::title_bar_input(std::int32_t width, std::uint32_t dpi) const
{
    return core::TitleBarInput{
        width, dpi, state_.tab_count(), state_.active_tab(), state_.tab_scroll(), state_.hovered()};
}

std::optional<CompositionView> EditorController::composed() const
{
    if (command_line_active())
    {
        return std::nullopt;
    }
    return composition_view_of(state_.composition());
}

std::optional<char> EditorController::recording_name() const
{
    const auto &recording = state_.vim().macro_recording;
    if (state_.mode() != core::EditMode::vim || !recording.has_value())
    {
        return std::nullopt;
    }
    // core は名前を小文字で持ち、追記（`qA`）は append の印で持つ。Vim は打った名前の
    // まま `recording @A` と出すので、追記なら大文字へ戻す。
    const char name = recording.value().name;
    return recording.value().append ? static_cast<char>(name - 'a' + 'A') : name;
}

std::optional<EditorController::SearchPattern> EditorController::search_pattern() const
{
    // 入力中は入力のパターンで塗る。解析できなければ何も塗らない（ADR 0041 の決定 4）。
    const core::SearchLine *typing = previewed_line(state_);
    if (state_.mode() == core::EditMode::vim && typing != nullptr)
    {
        auto parsed = typed_pattern(*typing, core::search_case(state_.settings()));
        if (!parsed.has_value())
        {
            return std::nullopt;
        }
        return SearchPattern{std::move(parsed).value()};
    }
    const auto &remembered = state_.vim().last_search;
    if (state_.mode() != core::EditMode::vim ||
        state_.vim().highlight != core::VimSearchHighlight::on || !remembered.has_value())
    {
        return std::nullopt;
    }
    const auto &parsed = remembered.value().parsed();
    if (!parsed)
    {
        return std::nullopt;
    }
    return SearchPattern{std::cref(parsed.value())};
}

const core::VimPattern &EditorController::frame_pattern(const core::VimPattern &pattern)
{
    return pattern;
}

const core::VimPattern *EditorController::pattern_of(const std::optional<SearchPattern> &pattern)
{
    return pattern.has_value() ? std::visit([](const auto &value) { return &frame_pattern(value); },
                                            pattern.value())
                               : nullptr;
}

const core::VimPattern &
EditorController::frame_pattern(std::reference_wrapper<const core::VimPattern> pattern)
{
    return pattern.get();
}

// 見えている 1 行ぶんの表示値。選択と検索は所有済み本文の同じ LineSpanEvaluation で投影する
// （ADR 0037 / 0095）。塗る面を持たない長さ 0 の一致は span が absent を返すので落ちる。
// 描画用の行は core の display_line 1 本で作る（ADR 0040 の決定 2）。桁は本文の桁のまま渡す。
LineView EditorController::line_view(const PaneSource &source, LineView view,
                                     const core::OffsetRange &range, core::Offset caret) const
{
    const core::TextBuffer &text = source.state.text();
    const core::VimPattern *pattern = source.pattern;
    const auto line = view.number;
    // 巨大な行は窓の中の本文だけを持つ。選択と当たりは窓の始まりから投影し、窓が行末に届かなければ
    // 窓の終わりを行末とみなす（ADR 0139 の限界）。
    const core::Offset start{text.line_start(line).value + view.window.byte};
    const core::Offset window_end{start.value + view.text.size()};
    const core::Offset terminator =
        window_end == text.line_end(line) ? text.line_terminator_end(line) : window_end;
    LineSpanEvaluation projection(view.text, start, terminator);
    view.selection = projection.span(range);
    if (pattern == nullptr)
    {
        return view;
    }
    // 今の一致はキャレットを含む一致。incsearch の入力中は preview の当たりを含む一致（ADR 0041
    // の決定 4）。全一致の面は hlsearch が on のときだけで、off の入力中は今の当たりの枠だけ
    // （Vim と同じ）。入力中でなければ off の search_pattern は何も返さない。
    for (const auto &found : shown_matches(text, view, *pattern))
    {
        const core::SelectionSpan span = projection.span(found);
        if (span.presence != core::SelectionPresence::present)
        {
            continue;
        }
        view.matches.push_back(span);
        if (found.begin.value <= caret.value && caret.value < found.end.value)
        {
            view.current_match = span;
        }
    }
    if (state_.vim().highlight != core::VimSearchHighlight::on)
    {
        view.matches.clear();
    }
    return view;
}

// 枠の見えている行（ADR 0148 の決定 8）。操作中の枠ももう 1 つの枠もこの 1 本を通る。スクロールは
// 呼び手（frame）が解決済み。
std::expected<std::vector<LineView>, TextLayoutFailure>
EditorController::visible_lines(const PaneSource &source, std::optional<SyntaxStateCache> &cache,
                                std::size_t &lexed) const
{
    const EditorState &state = source.state;
    const ScrollState &scroll = state.scroll();
    const std::size_t total = state.text().line_count();
    const std::size_t first = std::min(scroll.first_visible.value, total);
    const std::size_t last =
        std::min(first + std::max<std::size_t>(scroll.visible_lines, 1) - 1, total);
    const core::Offset caret = source.pattern != nullptr && first <= last
                                   ? previewed_offset(state).value_or(state.selection().caret)
                                   : state.selection().caret;
    std::vector<LineView> lines;
    if (first <= last)
    {
        lines.reserve(last - first + 1);
    }
    // 字句器を持つ言語だけ、見えている最初の行の行頭の状態を覚えか遡りから求める（ADR 0156 の
    // 決定 8・9・14）。持たない言語と `:syntax off` の文書（追補）は字句器も覚えも触らない。
    const SyntaxStateCache key{source.tab, state.language(), core::LineNumber{first},
                               core::LexState{}};
    auto lex = first <= last && state.syntax_enabled()
                   ? syntax_begin(state.text(), key, cache, lexed)
                   : std::nullopt;
    // text の表示の frame なので、構築は 1 升を port から読まない（格子でない）。
    const VisualTextView visual(state, ports_.text_layouts, view_geometry_);
    auto row = -static_cast<std::int64_t>(scroll.first_row);
    for (std::size_t number = first;
         number <= last && row < static_cast<std::int64_t>(scroll.visible_lines); ++number)
    {
        const core::LineNumber line{number};
        auto shown = line_view(source, visual.line(line),
                               block_row(source.block, line, source.range), caret);
        lex = colored_line(key.language, lex, shown, lexed);
        const auto count = screen_rows_of(shown);
        if (!count)
        {
            return std::unexpected(count.error());
        }
        shown.screen_row = row;
        shown.screen_rows = count.value();
        row += static_cast<std::int64_t>(count.value());
        lines.push_back(std::move(shown));
    }
    return lines;
}

// 巨大な行は折り返さないので 1 画面行（ADR 0139 の決定 2）。
std::expected<std::size_t, TextLayoutFailure>
EditorController::screen_rows_of(const LineView &shown) const
{
    if (view_geometry_.width <= 0 || shown.window.active)
    {
        return 1;
    }
    return ports_.text_layouts.rows(
        TextLayoutRequest{presented_text(shown), state_.settings(), view_geometry_});
}

GridLineMarks EditorController::grid_marks(const PaneSource &source, core::LineNumber line,
                                           const core::OffsetRange &visible,
                                           const core::OffsetRange &selection) const
{
    const core::VimPattern *pattern = source.pattern;
    GridLineMarks marks{selection, {}, std::nullopt, 0};
    if (pattern == nullptr)
    {
        return marks;
    }
    // 照合は見えている画面行の本文の範囲に一致の長さの上限ぶんの余裕を前後に足した窓だけで、行の
    // 全体を写さない（#556）。余裕の中で始まる一致は見えている範囲に届かないので、窓の端の文脈で
    // 偽って当たっても塗られない。窓の始まりの前と終わりの後ろの 1 code point を文脈に読み、
    // `^` `$` は行の端でだけ当たる。塗るのは見えている画面行の升へ写した分だけ（GridLineRows）。
    const core::TextBuffer &text = source.state.text();
    const core::OffsetRange content{text.line_start(line), text.line_end(line)};
    const auto window = grid_search_window(
        content, visible, pattern->longest_bytes().value_or(grid_unbounded_margin));
    const auto read = grid_search_window(content, window, grid_context_bytes);
    const auto bytes = text.text_range(read.begin, read.end);
    marks.searched = bytes.size();
    const core::Offset caret =
        previewed_offset(source.state).value_or(source.state.selection().caret);
    for (const auto &found :
         grid_window_matches(bytes,
                             core::OffsetRange{core::Offset{window.begin.value - read.begin.value},
                                               core::Offset{window.end.value - read.begin.value}},
                             read.begin.value, *pattern))
    {
        if (found.begin.value >= found.end.value)
        {
            continue;
        }
        marks.matches.push_back(found);
        if (found.begin.value <= caret.value && caret.value < found.end.value)
        {
            marks.current = found;
        }
    }
    if (state_.vim().highlight != core::VimSearchHighlight::on)
    {
        marks.matches.clear();
    }
    return marks;
}

std::optional<TextLayoutFailure>
EditorController::grid_visible_rows(const PaneSource &source,
                                    std::optional<SyntaxStateCache> &cache,
                                    std::vector<GridRowView> &grid, GridTally &tally) const
{
    const EditorState &state = source.state;
    const VisualTextView view(state, ports_.text_layouts, view_geometry_);
    auto cells = view.grid_cells();
    if (!cells)
    {
        return cells.error();
    }
    const auto caret = view.grid_point(
        cells.value(),
        core::TextCaret{state.text().position_of(state.selection().caret), state.caret_affinity()});
    const ScrollState scroll = state.scroll();
    // 画面の下に半分かかる画面行も描く（text と同じ）。
    const auto limit = scroll.visible_lines + 1;
    std::size_t screen = 0;
    auto first_row = scroll.first_row;
    const core::LineNumber first{std::max<std::size_t>(scroll.first_visible.value, 1)};
    // 字句器は text の表示と同じ遡りと覚えから（#839・ADR 0156 の決定 8・9・14）。
    auto lex =
        first.value <= state.text().line_count() && state.syntax_enabled()
            ? syntax_begin(state.text(),
                           SyntaxStateCache{source.tab, state.language(), first, core::LexState{}},
                           cache, tally.lexed)
            : std::nullopt;
    for (core::LineNumber line = first; line.value <= state.text().line_count() && screen < limit;
         ++line.value)
    {
        const auto count = cells.value().rows(line);
        const auto from = std::min(first_row, count - 1);
        const auto take = std::min(count - from, limit - screen);
        const core::OffsetRange visible{
            cells.value().byte_of(line, core::GridPoint{from, 0}),
            cells.value().byte_of(line, core::GridPoint{from + take, 0})};
        auto marks = grid_marks(source, line, visible, block_row(source.block, line, source.range));
        tally.searched += marks.searched;
        tally.lexed += lex.has_value() ? 1 : 0;
        lex = grid_colored_line(state, lex, line, marks.colors);
        GridLineRows(GridLineSource{state, cells.value(), line, from, from + take,
                                    static_cast<std::int64_t>(screen), visible, std::move(marks),
                                    caret})
            .append_to(grid);
        screen += take;
        first_row = 0;
    }
    tally.scanned = cells.value().scanned();
    return std::nullopt;
}

DocumentView EditorController::active_document_view() const
{
    const auto &document = state_.document();
    const auto save_state = save_state_of(document, state_.history().position());
    return DocumentView{core::tab_title_for(document.path, save_state), document.path,
                        document.encoding, save_state, state_.last_failure()};
}

core::GuideContext EditorController::guide_context() const noexcept
{
    if (state_.settings().guide == core::GuideVisibility::hidden ||
        state_.command_input().has_value() || state_.command_message().has_value() ||
        recording_name().has_value() || state_.composition().has_value())
    {
        return core::GuideContext::hidden;
    }
    if (!state_.document().path.has_value() && state_.text().size_bytes() == 0 &&
        state_.history().size() == 0)
    {
        return core::GuideContext::untouched_untitled;
    }
    return core::GuideContext::other;
}

core::EditorSettings EditorController::shown_settings() const
{
    const auto &input = state_.command_input();
    const auto focused =
        input.has_value() ? focused_command_of(input.value()) : std::optional<std::string>{};
    if (!focused.has_value())
    {
        return state_.settings();
    }
    const auto evaluated =
        core::evaluate_ex(focused.value(), state_.settings(), state_.appearance(), state_.themes());
    if (!evaluated)
    {
        return state_.settings();
    }
    const auto &changed = evaluated.value().settings;
    if (!changed.has_value())
    {
        return state_.settings();
    }
    // 最初の段は配色だけを重ねる（ADR 0150 の決定 4）。字体と字の大きさは保存した値のまま。
    auto shown = state_.settings();
    shown.theme = changed.value().theme;
    return shown;
}

EditorDelivery EditorController::delivery() const
{
    auto settings = shown_settings();
    const auto &theme = core::selected_theme(settings, state_.appearance());
    return EditorDelivery{theme.appearance,
                          state_.mode(),
                          state_.vim().mode,
                          ime_stance_of(state_.mode(), state_.vim().mode, state_.command_input()),
                          state_.command_input().transform(input_line_prompt_of),
                          composed(),
                          command_composed(),
                          active_document_view(),
                          std::move(settings),
                          state_.settings_failure(),
                          command_message(),
                          state_.closing(),
                          state_.close_request(),
                          state_.operation_request(),
                          state_.display_question(),
                          layout_failure_,
                          true,
                          std::nullopt};
}

// 頼まれた追従を解決してから作る（ADR 0128 の決定 3）。解決の失敗は failure に載る。
EditorFrame EditorController::frame()
{
    const ScrollState scroll = framed_scroll();
    auto delivered = delivery();
    const auto caret = state_.text().position_of(state_.selection().caret);
    const auto &theme = core::selected_theme(delivered.settings, delivered.appearance);
    auto tabs = tab_views(state_, delivered.document);
    const auto encoding = delivered.document.encoding;
    // 問いの答えを待つ間は本文の行を読まず文字組みもしない（ADR 0140 の決定 2・renderer は
    // display_pending を見て本文を描かない）。格子では画面行が grid に載る（決定 5）。
    const bool pending = display_pending(state_.display_inquiry());
    const bool gridded =
        !pending && state_.display() == core::DisplayForm::grid && view_geometry_.width > 0;
    const auto pattern = search_pattern();
    const PaneSource source{state_, highlighted_range(), block_selection(), pattern_of(pattern),
                            state_.autosave_ids().at(state_.active_tab())};
    std::size_t lexed = 0;
    auto lines = pending || gridded
                     ? std::expected<std::vector<LineView>, TextLayoutFailure>{}
                     : visible_lines(source, syntax_cache(state_.split().active_side), lexed);
    // frame の失敗は描けないものだけ（契約の破れは配達だけに載る・ADR 0133 の追補 2026-10-18）。
    const auto failure = lines ? drawing_failure(layout_failure_) : std::optional{lines.error()};
    EditorFrame result{std::move(lines).value_or(std::vector<LineView>{}),
                       CaretView{caret, caret_shape_for(delivered.mode, delivered.vim_mode),
                                 state_.caret_affinity()},
                       scroll.first_visible,
                       state_.text().line_count(),
                       delivered.appearance,
                       theme.ui,
                       theme.body,
                       delivered.mode,
                       delivered.vim_mode,
                       delivered.ime,
                       core::mode_label(delivered.mode, delivered.vim_mode),
                       recording_name(),
                       std::move(delivered.composition),
                       std::move(delivered.command_composition),
                       std::move(delivered.document),
                       status_items_of(state_, caret, encoding),
                       std::move(delivered.settings),
                       std::move(delivered.settings_failure),
                       command_line_view(),
                       std::move(delivered.command_message),
                       command_palette_view(),
                       std::move(tabs),
                       state_.active_tab(),
                       state_.tab_scroll(),
                       state_.hovered(),
                       delivered.closing,
                       delivered.close_request,
                       delivered.operation_request,
                       delivered.display_question,
                       core::operation_guide(guide_context(), delivered.mode),
                       failure,
                       scroll.horizontal,
                       state_.display(),
                       pending};
    finish_frame(result, source, gridded, lexed);
    return result;
}

// frame の後半。格子の画面行・字句器の数・もう 1 つの枠を載せる。
void EditorController::finish_frame(EditorFrame &result, const PaneSource &source, bool gridded,
                                    std::size_t lexed)
{
    if (gridded)
    {
        GridTally tally{0, 0, lexed};
        const auto grid_failure =
            grid_visible_rows(source, syntax_cache(state_.split().active_side), result.grid, tally);
        result.layout_failure = grid_failure.has_value() ? grid_failure : result.layout_failure;
        result.grid_scanned = tally.scanned;
        result.grid_searched = tally.searched;
        lexed = tally.lexed;
    }
    result.syntax_lexed = lexed;
    add_other_pane(result, source.pattern);
    result.explorer = explorer_view();
}

std::optional<SyntaxStateCache> &EditorController::syntax_cache(core::PaneSide side) noexcept
{
    switch (side)
    {
    case core::PaneSide::first:
        return first_syntax_;
    case core::PaneSide::second:
        return second_syntax_;
    }
    std::unreachable();
}

std::optional<SyntaxStateCache> &EditorController::passive_syntax_cache() noexcept
{
    switch (state_.split().active_side)
    {
    case core::PaneSide::first:
        return second_syntax_;
    case core::PaneSide::second:
        return first_syntax_;
    }
    std::unreachable();
}

// もう 1 つの枠の表示値（ADR 0148 の決定 8）。行は操作中の枠と同じ visible_lines /
// grid_visible_rows を、もう 1 つの枠を操作中の枠の形で読む写しから作る。幅は両枠で等しいので
// 同じ寸法を渡す（決定 1）。選択は無く、検索の面は両枠に塗る（限界）。
void EditorController::add_other_pane(EditorFrame &result, const core::VimPattern *pattern)
{
    result.layout = state_.split().layout;
    result.active_side = state_.split().active_side;
    const auto &other = state_.split().other;
    if (!other.has_value())
    {
        return;
    }
    std::visit([this, &result, pattern](const auto &view)
               { this->add_other_view(result, pattern, view); }, other.value());
}

// プレビューの枠は値の写しだけで、本文を歩かない（ADR 0155 の決定 11）。追従の間の錨は本文の
// 先頭行の行頭（決定 7）。
void EditorController::add_other_view(EditorFrame &result, const core::VimPattern *,
                                      const PreviewView &view) const
{
    const core::Offset anchor =
        view.follows ? state_.text().line_start(result.first_visible) : view.anchor;
    result.other = PreviewFrame{view.document, anchor, view.notice};
}

void EditorController::add_other_view(EditorFrame &result, const core::VimPattern *pattern,
                                      const PaneView &view)
{
    const auto pane = state_.other_pane_state();
    if (!pane.has_value())
    {
        return;
    }
    const auto &ids = state_.autosave_ids();
    const auto found = std::ranges::find(ids, view.tab);
    if (found == ids.end())
    {
        return;
    }
    const auto position = static_cast<std::size_t>(std::distance(ids.begin(), found));
    const EditorState &state = pane.value();
    const PaneSource source{state, core::selection_range(state.selection()), std::nullopt, pattern,
                            view.tab};
    PaneFrame other{{},
                    CaretView{state.text().position_of(state.selection().caret), result.caret.shape,
                              state.caret_affinity()},
                    state.scroll().first_visible,
                    state.text().line_count(),
                    state.scroll().horizontal,
                    state.display(),
                    {},
                    result.tabs.at(position)};
    // 問いの答えを待つ文書は本文を読まない（操作中の枠と同じ・ADR 0140 の決定 2）。
    const bool pending = display_pending(state.display_inquiry());
    std::optional<TextLayoutFailure> failure;
    if (!pending && state.display() == core::DisplayForm::grid && view_geometry_.width > 0)
    {
        GridTally tally;
        failure = grid_visible_rows(source, passive_syntax_cache(), other.grid, tally);
    }
    else if (!pending)
    {
        // もう 1 つの枠は操作中の枠と別の側の覚えを使う（ADR 0156 の決定 9）。
        std::size_t lexed = 0;
        auto lines = visible_lines(source, passive_syntax_cache(), lexed);
        failure = lines ? std::nullopt : std::optional{lines.error()};
        other.lines = std::move(lines).value_or(std::vector<LineView>{});
    }
    result.layout_failure = result.layout_failure.has_value() ? result.layout_failure : failure;
    result.other = std::move(other);
}
} // namespace nenenib::application
