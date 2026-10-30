#pragma once

#include "FilePath.hpp"
#include "Language.hpp"
#include "LanguageTable.hpp"
#include "LineEnding.hpp"
#include "SaveState.hpp"
#include "TextEncoding.hpp"

#include <cstddef>
#include <optional>
#include <utility>

namespace nenenib::application
{
// いま開いている文書（経路・文字コード・保存時点の履歴の位置・保存の形）。所有者は EditorState
// （ARC-004）。どの欄も互いに独立して妥当な値なので公開 aggregate（ADR 0007 / CPP-003）。
struct Document
{
    std::optional<core::FilePath> path;
    core::TextEncoding encoding;
    std::optional<std::size_t> saved_position;
    // 保存の改行の形（ADR 0010 の追補 #1049 の決定 1）。値なしは本文の改行の形（TextBuffer の値・
    // 読んだときの形で、Enter や `o` が挿す形）のまま。選ぶ操作が変えるのはこの値だけで、本文の
    // バイト列は変えず、undo にも載せない。
    std::optional<core::LineEnding> line_ending = std::nullopt;
    // 保存した時点の文字コードと改行の形（決定 1）。値なしは「今の値と同じ」。形だけを変えた文書の
    // 未保存の印はこの 2 つで決まり、保存が成功すると両方とも値なしに戻る。
    std::optional<core::TextEncoding> saved_encoding = std::nullopt;
    std::optional<core::LineEnding> saved_line_ending = std::nullopt;
};

// 未保存かどうかは、履歴の位置と保存時点の一致と、保存の形と保存時点の形の一致で決まる（ADR 0010
// の決定 7・追補 #1049 の決定 1）。判定はこの 1 か所。
[[nodiscard]] inline core::SaveState save_state_of(const Document &document,
                                                   std::size_t position) noexcept
{
    if (document.saved_position.has_value() && document.saved_position.value() == position &&
        !document.saved_encoding.has_value() && !document.saved_line_ending.has_value())
    {
        return core::SaveState::saved;
    }
    return core::SaveState::modified;
}

// 保存の改行の形（決定 1）。本文の形は TextBuffer が持つので、呼び手が渡す。
[[nodiscard]] inline core::LineEnding save_line_ending_of(const Document &document,
                                                          core::LineEnding body) noexcept
{
    return document.line_ending.value_or(body);
}

// 文字コードを替えた文書（決定 1）。保存時点の値に戻したら形の未保存の印は消える。
[[nodiscard]] inline Document with_encoding(const Document &document, core::TextEncoding encoding)
{
    Document next = document;
    const core::TextEncoding saved = document.saved_encoding.value_or(document.encoding);
    next.encoding = encoding;
    next.saved_encoding = encoding == saved ? std::nullopt : std::optional{saved};
    return next;
}

// 保存の改行の形を替えた文書（決定 1）。body は本文の改行の形。
[[nodiscard]] inline Document with_line_ending(const Document &document, core::LineEnding body,
                                               core::LineEnding ending)
{
    Document next = document;
    const core::LineEnding saved =
        document.saved_line_ending.value_or(save_line_ending_of(document, body));
    next.line_ending = ending;
    next.saved_line_ending = ending == saved ? std::nullopt : std::optional{saved};
    return next;
}

// 保存の形の 1 つの選択を置いた文書（決定 2）。std::visit で選ぶ 2 つの口。
[[nodiscard]] inline Document with_format(const Document &document, core::LineEnding /*body*/,
                                          core::TextEncoding encoding)
{
    return with_encoding(document, encoding);
}

[[nodiscard]] inline Document with_format(const Document &document, core::LineEnding body,
                                          core::LineEnding ending)
{
    return with_line_ending(document, body, ending);
}

// 保存が成功した文書（ADR 0010 の決定 7・追補 #1049 の決定 1）。保存の改行の形は保ち、保存時点の
// 形は今の形になる。
[[nodiscard]] inline Document saved_document(const Document &document, core::FilePath path,
                                             core::TextEncoding encoding, std::size_t position)
{
    return Document{std::move(path),      encoding,     position,
                    document.line_ending, std::nullopt, std::nullopt};
}

// 文書のパスから決まる言語（ADR 0154 の決定 5）。名前の無い文書は plain。開く・名前が変わる保存・
// まだ読んでいない文書を読むときの束はどれもここを通り、拡張子は core の表だけが見る（決定 2）。
[[nodiscard]] inline core::Language language_of(const Document &document) noexcept
{
    return document.path.has_value() ? core::language_of_path(document.path.value().text())
                                     : core::Language::plain;
}

// 新しい編集は redo の列を切る。保存時点がその先にあったら、もう戻れないので空にする（決定 7）。
[[nodiscard]] inline Document after_edit_at(const Document &document, std::size_t position)
{
    if (document.saved_position.has_value() && document.saved_position.value() > position)
    {
        Document next = document;
        next.saved_position = std::nullopt;
        return next;
    }
    return document;
}
} // namespace nenenib::application
