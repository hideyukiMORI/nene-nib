#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// 文とタグのテキストオブジェクトを範囲関数 vim_text_object_range で 16 回引く（#732）。
// 文はピリオドの無い巨大な 1 行の `is` と、`.` が 50 字ごとの行の `as`。タグは入れ子 200 段の
// 一番外の段からの `it` と、閉じの無い開き 500 個の後ろからの `at`（失敗の道）。
enum class TextObjectWorkload : std::uint8_t
{
    sentence_giant,
    sentence_dense,
    tag_nested,
    tag_unclosed
};
} // namespace nenenib::tests::performance
