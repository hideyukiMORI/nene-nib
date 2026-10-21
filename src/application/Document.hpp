#pragma once

#include "FilePath.hpp"
#include "Language.hpp"
#include "LanguageTable.hpp"
#include "SaveState.hpp"
#include "TextEncoding.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::application
{
// いま開いている文書（経路・文字コード・保存時点の履歴の位置）。所有者は EditorState（ARC-004）。
// 3 つは互いに独立して妥当な値なので公開 aggregate（ADR 0007 / CPP-003）。
struct Document
{
    std::optional<core::FilePath> path;
    core::TextEncoding encoding;
    std::optional<std::size_t> saved_position;
};

// 未保存かどうかは履歴の位置と保存時点の一致で決まる（ADR 0010 の決定 7）。
[[nodiscard]] inline core::SaveState save_state_of(const Document &document,
                                                   std::size_t position) noexcept
{
    if (document.saved_position.has_value() && document.saved_position.value() == position)
    {
        return core::SaveState::saved;
    }
    return core::SaveState::modified;
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
        return Document{document.path, document.encoding, std::nullopt};
    }
    return document;
}
} // namespace nenenib::application
