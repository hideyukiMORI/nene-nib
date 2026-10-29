#pragma once

#include "TextLayoutFailure.hpp"

#include "Appearance.hpp"
#include "CompositionView.hpp"
#include "DocumentView.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "ImeStance.hpp"
#include "InputLinePrompt.hpp"
#include "MapTimer.hpp"
#include "SettingsIssue.hpp"
#include "VimMode.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::application
{
// 意図を直ちに写した所有スナップショット。本文の行や描画の仕事は含まない（ADR 0084）。
struct EditorDelivery
{
    core::Appearance appearance;
    core::EditMode mode;
    core::VimMode vim_mode;
    ImeStance ime;
    // 開いている入力行の種類（frame の command_line の prompt と同じ値）。ui は構えが
    // closed_once のままでも、入力行の種類が変わったとき（Ex の確定で面が開くなど）は
    // IME を 1 度閉じる（ADR 0061 の決定 2・#635 の差し戻し 1）。
    std::optional<core::InputLinePrompt> input_line;
    std::optional<CompositionView> composition;
    std::optional<CompositionView> command_composition;
    DocumentView document;
    core::EditorSettings settings;
    std::optional<SettingsIssue> settings_failure;
    std::optional<core::DisplayText> command_message;
    bool closing;
    std::optional<std::size_t> close_request;
    std::optional<core::EditorOperation> operation_request;
    // 開いた文書に格子表示を聞く（ADR 0140 の決定 2）。close_request と同じく 1 意図の 1 回だけ
    // 立つ。ui は意図を送った結果を受ける 1 か所でこれを見て問いを出す。
    bool display_question = false;
    std::optional<TextLayoutFailure> layout_failure{};
    // 描き直しが要るか。合図（AutosaveTick / PreviewTick）が退避もプレビューの要求も出さず何も
    // 変えなかったときだけ false で、ui はそのとき無効化を積まない（ADR 0155 の追補・#813）。
    // ほかの意図は true。
    bool repaint = true;
    // Vim の `map` の待ちのタイマー（ADR 0176 の決定 4）。この意図が待ちを始めた・積み替えたら
    // start、待ちを流して空にしたら stop、変えなかったら空。
    std::optional<MapTimer> map_timer = std::nullopt;
};
} // namespace nenenib::application
