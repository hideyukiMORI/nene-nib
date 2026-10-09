#pragma once

#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "HistoryFailure.hpp"

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <vector>

namespace nenenib::core
{
// 直前のEditにeditを畳んだ結果（ADR 0015 / 0028）。inserted範囲内の編集と、その直前の
// 削除を合成する。範囲から離れていれば空を返す。INSERTのundo単位を作る唯一の経路。
// 畳んだ結果の restore は previous の値（最初の編集の瞬間のキャレット・ADR 0052 の決定 3）。
[[nodiscard]] std::optional<Edit> absorbed(const Edit &previous, const Edit &edit);

// undo / redo の履歴（ADR 0009 の決定 3）。所有者は application の EditorState（ARC-004）。
// 公開状態は不変なので、履歴の位置を動かす undone / redone は次の履歴を返す（ARC-005）。
// undo / redo は「戻すべき編集」を見るだけで位置を動かさない。2 つを組で使う。
class EditHistory final
{
  public:
    [[nodiscard]] static EditHistory empty();
    [[nodiscard]] EditHistory pushed(const Edit &edit, EditBoundary boundary) const;
    [[nodiscard]] std::expected<Edit, HistoryFailure> undo() const;
    [[nodiscard]] std::expected<Edit, HistoryFailure> redo() const;
    // 末尾の単位を閉じた次の履歴。保存の直後に呼ぶと、続く入力が保存時点の単位に混ざらない
    // （ADR 0010 の決定 7）。位置は動かさない。
    [[nodiscard]] EditHistory sealed() const;
    // 本文に適用済みの index 番目の編集。範囲の外なら空。再生が積んだ最初の編集の restore を
    // 畳んだ単位へ写すために読む（ADR 0052 の決定 3）。
    [[nodiscard]] std::optional<Edit> applied(std::size_t index) const;
    [[nodiscard]] EditHistory undone() const;
    [[nodiscard]] EditHistory redone() const;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t position() const noexcept;

  private:
    // 外部 Edit は push で防御コピーし、確定済み本文は私有の const 値として共有する（ADR 0085）。
    using Entry = std::shared_ptr<const Edit>;
    EditHistory(std::vector<Entry> edits, std::size_t position, EditBoundary tail);
    std::vector<Entry> edits_;
    // edits_[0, position_) が本文に適用済み。position_ より後ろは redo で戻せる編集。
    std::size_t position_;
    // 末尾の単位がまだ続いているか。separate で閉じた単位には次の入力も混ざらない（ADR 0009）。
    EditBoundary tail_;
};
} // namespace nenenib::core
