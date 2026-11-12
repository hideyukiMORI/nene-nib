#pragma once

#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "HistoryDirection.hpp"
#include "HistoryFailure.hpp"
#include "HistoryUnit.hpp"
#include "VimChange.hpp"

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <vector>

namespace nenenib::core
{
// 直前のEditにeditを畳んだ結果（ADR 0015 / 0028）。inserted範囲内の編集と、その直前の
// 削除と、その終わりから後ろの削除（INSERT の `<Del>`・#1325）を合成する。範囲から離れていれば
// 空を返す。INSERTのundo単位を作る唯一の経路。
// 畳んだ結果の restore は previous の値（最初の編集の瞬間のキャレット・ADR 0052 の決定 3）。
[[nodiscard]] std::optional<Edit> absorbed(const Edit &previous, const Edit &edit);

// undo / redo の履歴（ADR 0009 の決定 3）。所有者は application の EditorState（ARC-004）。
// 公開状態は不変なので、履歴の位置を動かす undone / redone は次の履歴を返す（ARC-005）。
// undo / redo は「戻すべき編集」を見るだけで位置を動かさない。2 つを組で使う。
class EditHistory final
{
  public:
    [[nodiscard]] static EditHistory empty();
    // 本文を変えた編集を積む（change は changed）。通常モードと Vim の効果の置き換えが通る。
    [[nodiscard]] EditHistory pushed(const Edit &edit, EditBoundary boundary) const;
    // change の印つきで積む（ADR 0181 の決定 3）。edit が空なら本文に触れない「空の単位」で、前の
    // 単位に畳まれず（absorbed にも coalesce にも入れない）、必ず新しい単位を開く。空の単位の後ろに
    // INSERT の打鍵（absorb）が続けば、その打鍵は空の単位に畳まれて 1 単位になる（`C` の後の打鍵が
    // 1 回の `u` で戻るのと同じ・Vim の op_change の u_save と edit() は 1 単位）。
    // 畳んだ単位の change は、どちらかが changed なら changed。空の単位へ通常モードの
    // 打鍵（coalesce）は畳まない。
    [[nodiscard]] EditHistory pushed(const Edit &edit, EditBoundary boundary,
                                     VimChange change) const;
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
    // direction の向きに続く空の単位を全部飛ばした次の履歴（ADR 0181 の追補 9・#1535）。次の
    // undo / redo は本文を変える単位になる。その向きに本文を変える単位が無ければ端の失敗
    // （nothing_to_undo / nothing_to_redo）で、空の単位も動かさない。通常モード（Vim でないモード）
    // の Ctrl+Z / Ctrl+Y の入口だけが呼び、飛ばした件ごとに undone / redone
    // と同じく位置・改訂番号・ 本文の位置を動かす。Vim の `u` `<C-r>` は呼ばない。
    [[nodiscard]] std::expected<EditHistory, HistoryFailure>
    past_empty_units(HistoryDirection direction) const;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t position() const noexcept;
    // 改訂番号（ADR 0137 の決定 2）。編集（前の単位にまとめられたときも）・undo・redo のたびに
    // 1 増え、減らない。sealed では変わらない。位置はまとめられた編集と undo の後の別の編集で
    // 同じ値に戻るので、「前に見たときから変わったか」は位置ではなくこれで比べる。
    [[nodiscard]] std::size_t revision() const noexcept;
    // 本文の位置（ADR 0181 の決定 4）。[0, position) にある changed の単位の数。saved_only の
    // 単位は数えない。保存時点と比べる値で、判定は application の save_state_of の 1 か所。
    // 値は件ごとに数え直さず、積む・畳む・戻す・やり直すの 4 つで足し引きして持つ（O(1)）。
    [[nodiscard]] std::size_t content_position() const noexcept;

  private:
    // 外部 Edit は push で防御コピーし、確定済み本文は私有の const 値として共有する（ADR 0085）。
    using Entry = std::shared_ptr<const HistoryUnit>;
    // 履歴は固定の件数の塊の不変の列（ADR 0180 の決定 2・ADR 0044 の chunk の列と同じ考え方）。
    // 値の写し（状態の with_* のたび）は束の参照を 1 つ増やすだけで、足すときに写すのは塊の列と
    // 末尾の塊だけ（件数に比例しない）。
    using Block = std::vector<Entry>;
    using Blocks = std::vector<std::shared_ptr<const Block>>;
    EditHistory(std::shared_ptr<const Blocks> blocks, std::size_t size, std::size_t position,
                EditBoundary tail, std::size_t revision, std::size_t content);
    // 0 から count 件を残し、その後ろに entry を足した次の束。
    [[nodiscard]] std::shared_ptr<const Blocks> kept_with(std::size_t count, Entry entry) const;
    [[nodiscard]] const HistoryUnit &entry_at(std::size_t index) const;
    std::shared_ptr<const Blocks> blocks_;
    // 束の中の編集の数。塊の列は size_ 件ちょうどを持つ。
    std::size_t size_;
    // [0, position_) が本文に適用済み。position_ より後ろは redo で戻せる編集。
    std::size_t position_;
    // 末尾の単位がまだ続いているか。separate で閉じた単位には次の入力も混ざらない（ADR 0009）。
    EditBoundary tail_;
    std::size_t revision_;
    // [0, position_) の changed の単位の数（content_position）。
    std::size_t content_;
};
} // namespace nenenib::core
