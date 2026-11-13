#pragma once

#include "DisplayLanguage.hpp"

#include <string_view>

namespace nenenib::core
{
// 「保存しますか」の箱の文（ADR 0133 の追補 #1282）。ボタンの文言も OS に任せず自分で持つ。
// 文の名前の閉じた enum で、表示言語の表（#1019 の ui_texts）へはこの値ごと移す。
enum class SaveQuestionText
{
    // 使う人が閉じる・タブを閉じるときの問い（保存する／保存しない／キャンセル）。
    unsaved_question,
    // 描画を失った後の問い（保存する／保存しない・取消は無い・ADR 0133 の決定 3）。
    rendering_lost_question,
    save_button,
    discard_button,
    cancel_button,
};

// 文は表示言語の文の表（UiTexts・#1019）から引く。ボタンの `&` は箱のアクセスキー（MessageBox の
// Y / N と同じ）。
[[nodiscard]] std::string_view save_question_text(SaveQuestionText text,
                                                  DisplayLanguage language) noexcept;
// 言語の無い形は日本語の文（ui の試験の形・T1e で言語の形へ寄せる）。
[[nodiscard]] std::string_view save_question_text(SaveQuestionText text) noexcept;
} // namespace nenenib::core
