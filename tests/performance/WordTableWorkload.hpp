#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// 語の表を最初に作る 1 回の費用（#1868・ADR 0169 の追補 2026-11-18）。上限
// core::word_table_byte_limit から 4 KiB 引いた大きさの文書を開いた controller で、
// EditorController::insert_completion の 1 回を測る。first は表の無い最初の 1 回（表を作る道を
// 含む）、second は表を持った後の 2 回目（候補を集めるだけ）、rebuilt は 64 KiB
// （word_table_edit_limit）を超える編集と undo で表が捨てられた後の作り直しの 1 回。文書は
// 識別子の種類が多い ASCII のソース風（ascii）と、日本語を混ぜた文章（japanese）の 2 種。
enum class WordTableWorkload : std::uint8_t
{
    first_ascii,
    second_ascii,
    rebuilt_ascii,
    first_japanese,
    second_japanese,
    rebuilt_japanese
};
} // namespace nenenib::tests::performance
