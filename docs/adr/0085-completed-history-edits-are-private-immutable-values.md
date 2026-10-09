# ADR 0085 — 確定済み履歴の編集は私有の不変値として共有する

- 状態: 受理（工程1の設計、性能・製品統合の受理は別）
- 日付: 2026-10-09
- Issue: #323
- 影響する規則: ARC-001 / ARC-004 / ARC-005 / ARC-008 / CPP-003 / CPP-004 / CPP-007 / CPP-016 / QLT-001 / QLT-012 / QLT-013 / QLT-014

## 文脈

EditHistoryのpushed/sealed/undone/redoneは履歴列を値として写す。Editはremoved/insertedの所有stringを持つため、確定した大きな削除も続く打鍵ごとに全文複製される。#321/#322後も残るこの費用を、hideの固定入力の対応前後比較に従って分離する。

本ADRは親設計席の最小の履歴所有変更だけを確定する。通常/Vimのundo単位、restore、保存境界、redoの意味はADR0009/0010/0015/0028/0052のまま保つ。Vimレジスタと作成中の記録の所有は次工程で別に決める。

## 決定

1. Editの公開値とAPIは変えない。EditHistoryのprivate列だけを`std::vector<std::shared_ptr<const Edit>>`へ変える。aliasはprivate、pointer/reference/string_viewを返す公開生成口や読み口を増やさない。所有者は引き続きapplicationのEditorState。
2. pushedが外から受ける`const Edit&`は必ずコピーしてprivate const Editにする。raw Edit/stringをmove受理する公開最適化は作らず、呼出元に残ったdata()のmutable aliasが所有済みentryを変更できないようにする（CPP-003）。
3. 既存履歴の`[0, position)`はpointer列だけ複製し、redo suffixを従来どおり捨てる。tailが一致した末尾のconst Editを既存の唯一のfolded/absorbedで合成する。合成に成功したprivate local owned Editだけを新しいconst Editへmoveし、末尾一つを置換する。旧entryは変更しない。
4. undo/redo/appliedは従来どおりEditの所有コピーと同じエラー/不在を返す。sealed/undone/redoneは不変entryを共有した列を作り、positionとtailだけ既存規則で変える。coalesce/absorb、restore、保存で単位を閉じる規則、dirtyの位置判定を変更しない。
5. private constructorへの列にはnullptr entryを生成しない。入口は現在のempty/pushedだけで、単位移動も同じ列を使う。shared_ptrは直接dereferenceし、optionalはhas_value/valueで読む。
6. 工程1のscopeは確定済みEdit本文の複製除去だけ。現在成長中のinsertedの合成、entry数に比例するpointer列の複製、Vimレジスタの本文/記録鍵の複製は残す。可変cache・機能flag・旧実装切替・gate/baseline変更は作らない。#323全体完了とはしない。

## 対象検証と測定

- 既存CoreTestsのempty/travel/coalesce/absorbと、所有/二枝/redo切捨て/保存境界の追加契約を`--edit-history`へ集め、既定実行も同じ関数を呼ぶ。旧期待値・fixtureは変更しない。
- 長いremoved/insertedを持つ外部Editでpush後に元のdata alias/fieldsを書き換え、取り出したEditも変更して、旧履歴が不変であることを確認する。旧snapshotから二枝を作り、合成/undo/new pushでも元と他枝の本文・restore・位置が不変であることを確認する。単体でMiBを大量反復しない。
- 共有履歴は通常/Vim undoの共通基盤なので、Debug nib_tests/clang-tidy（並列2）、`--edit-history`、`--application`（既存Vim fixture1853を含む）だけを選ぶ。固定format、変更源のconformance/buildgraph、core/application symbols、Git/保護差分を確認する。工程変更だけでtabs/background/operations/incsearch等を再実行しない。
- 成功後clean commitから既存正典`eng/build-release.ps1 -Ref HEAD`（並列2）で製品Releaseを作る。GUI/正式速度と#322 before対本候補afterの同一#329 harness比較は親が行う。1MiB/16MiB削除後200入力の全文とUndo2回/Redo2回の検証は区間外、固定ABBAの全値を保存する。基準値採用/正式GUI benchの代用にしない。
- 親の固定測定中は解除通知までsource/ADR/testの編集とcommitだけ。configure/build/test/probe/GUIを始めない（2026-10-09の依頼書）。解除後に対象検証へ進む。QLT-001/012とADR0021に従い、成功済み結果は入力が不変なら工程間で再利用する。

## 限界と次工程

undo/redo/appliedが所有Editを返す時の本文コピーと、現在の単位を合成する時のstringコピーは残る。shared_ptrによる参照管理とpointer列の複製の費用もあり、利益は事前の推定では決めない。次のレジスタ/active record設計と#323全体の受理は別工程。保存schema、入力/IME/表示、undoの単位は不変。waiverなし。

作業木は`D:/NeNeNib/worktrees/323-shared-history`だけ。製品統合・親の測定参照・証拠収載が終わるまで保持し、親が未保存/ignored/唯一成果物/稼働参照/リンクを確認して整理する。branchとcommitは保持する。
