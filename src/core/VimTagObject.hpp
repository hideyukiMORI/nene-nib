#pragma once

#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimTextObjectOutcome.hpp"
#include "VimTextObjectRequest.hpp"

#include <cstddef>

namespace nenenib::core
{
// タグのテキストオブジェクト `it` `at`（ADR 0031 の決定 11・Issue #745）。範囲関数
// vim_text_object_range のタグの枝で、オペレータの後ろでも VISUAL でもこの答えを使う（ARC-001）。
//
// 定義は Vim の current_tagblock。後ろへ未対応の開きタグ `<name ...>` を探し（自己閉じの `<x/>` は
// 開きにならない・名前は問わず入れ子を数える）、その名前の `</name>` を前へ探す（名前は ASCII の
// 大文字小文字を畳む・同じ名前の開きを数える）。閉じが無いか、閉じが元の位置より前なら、さらに
// 後ろの開きで探し直す。回数は外側へ数える開きの数。`at` はタグを含み、`it` は開きの `>` の次から
// 閉じの `<` の手前まで。範囲は文字単位で、VISUAL は文字単位 VISUAL へ移る。`it` が今の選択と
// 同じ範囲になるときは `at` で選び直す。見つからなければ取消でキャレットも動かない。
// どれも Vim 9.1 で実測。走査はキャレットの周りの窓だけを読み、行を写さない。純関数。
[[nodiscard]] VimTextObjectOutcome vim_tag_object_range(const TextBuffer &text,
                                                        const Selection &selection,
                                                        VimTextObjectRequest request,
                                                        std::size_t count);
} // namespace nenenib::core
