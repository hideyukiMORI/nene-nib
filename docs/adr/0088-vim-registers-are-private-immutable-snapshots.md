# ADR 0088 — Vimレジスタ本文は私有の不変snapshotで共有する

- 状態: 受理（工程2の設計、性能・製品統合の受理は別）
- 日付: 2026-10-09
- Issue: #323
- 影響する規則: ARC-001 / ARC-004 / ARC-005 / CPP-003 / CPP-004 / CPP-007 / CPP-008 / CPP-011 / CPP-016 / QLT-001 / QLT-012 / QLT-013 / QLT-014

## 文脈

VimStateのコピーは無名・名前つき26本・数字10本・小削除・clipboardの本文を複製する。工程1の確定済み履歴共有後も、大きい名前つき本文を保持した通常打鍵の費用が残る。本文の意味、writerの順番、paste/replayの経路はADR0048/0050/0051のまま、所有だけを変える。

## 決定

1. coreの主要型VimRegisterSnapshotを同名ファイルに置く。privateのvariantは空本文のinline VimRegisterか、非空本文のshared_ptr<const VimRegister>だけを保持する。nullを生成しない。空でもkindとwidthを完全に維持し、空39slotにheapを確保しない。
2. 唯一の公開生成口from(const VimRegister&)は必ず防御コピーする。raw DTOのmove入力でも呼出元のdata()可変aliasを引き継がない。raw所有move、shared_ptr入力、暗黙変換、可変viewは公開しない。VimRegisterの意味や検証を増減せず、収納形式の不変条件に新しい想定内エラーを設けない。
3. value() const&だけがconst VimRegister&を貸す。const&&はdeleteする。snapshot同士のコピー/代入はpayloadを共有し、本文を変更しない。明示defaultのcopy ctor/copy assignmentで暗黙moveを抑える。rvalueからの構築/代入も共有copyとなり、元と先のvalue()は同じまま有効である。shared_ptrをmoveして元をnullにする実装は不可。raw DTOからの防御コピーとは別の不変条件として試験する。
4. VimStateのunnamed_register/small_delete/clipboardとVimNamedRegisters/VimNumberedRegistersの要素をsnapshotへ移す。VimRegisterはintentと外部入出力DTOとして維持する。公開VimState aggregate全体のprivate化や録画鍵列・`.`・counted INSERTの所有変更は本工程へ入れない。
5. snapshotに既定ctorを置かない。空の配列は1つの周辺templateとindex_sequenceでfromした空uninitializedをN個構築し、既存のnamed/numbered aggregateの空初期化を保つ。
6. vim_resting_state/fromの正典はsnapshotを受けて共有する。必要なraw overloadはfromから正典へ委譲する薄いwrapperだけ。既存snapshotをvalue()経由で毎打鍵再凍結しない。clipboard消去とほかの欄の保持は従来通り。
7. 読みはvalue()のconst参照、書き込み時だけfromで新snapshotへ置換する。registers_writtenの名指し→数字繰下げ→小削除→無名という順番、appendの失敗、clipboard load、macro finish、Storeの意味を維持する。一度凍結した編集値は複数slotへ共有できるが、writerの別経路を作らない。選択した値を返す既存raw APIと実際のpaste/replay時のコピーは維持できる。

## 対象検証と測定

新selector --vim-register-snapshotと既定実行は同じ契約関数を呼ぶ。空/非空/長い本文、空kind/width、raw mutable alias、snapshotのrvalue後の元/先、旧状態からの二枝、named/numbered/small delete/clipboard/appendの独立性、rvalue value()拒否を確認する。既存試験はアクセスだけ移行し期待値を変えない。

Debug nib_tests/NeNeNibをpinned clang-cl/clang-tidyで並列2。新selectorと--application（fixture1853を含む共通結合）、--vim-macro（保存/追記）、--vim-clipboard（境界）だけを選ぶ。format、File API付きconformance、core/application symbols、Git/whitespace、protected diffを記録する。旧12workloadは1byteも変更しない。

clean実装commitから既存eng/build-release.ps1 -Ref HEADで製品を作り、同じRelease cacheを明示configureしてnib_perf_probesだけ作る。窓・正式速度・同一harnessの固定前後比較と製品受理は親担当。測定中のbuild/runtime禁止と解除を守る。対象が不変の成功結果はQLT-001/012に従い再利用する。

## 限界と保持

共有pointer管理、実際のpaste/replayでのrawコピー、作成中の録画鍵列と`.`、counted INSERT本文のコピーは残る。利益は親の全値保存比較で判断し、#323全体完了とはしない。保存schemaと編集/Undo/registerの意味は不変、waiverなし。

作業木D:/NeNeNib/worktrees/323-shared-registersとout証拠は親の比較/統合参照が終わるまで保持する。親が取込・未保存/ignored・唯一成果物・稼働参照・リンクを絶対パスで確認し整理する。branchとcommitは保持する。
