# ADR 0118 — CIはマージ直前だけ、文書のみなら起動しない

- 状態: 受理
- 日付: 2026-10-12
- Issue: #430
- 根拠: hideの明示指定。README更新にもCIと長い検証手順を適用していた過剰を止める。
- 影響する規則: QLT-001 / QLT-007 / QLT-010 / QLT-012 / GIT-004

## 決定

CIは`eng/merge-pr.py --execute`によるマージ直前だけ起動し、文書のみの差分では起動しない。
PR作成・Ready・push・本文編集・merge後のイベントでは起動しない。

文書の範囲は`docs/`配下、ルートの`.md`、`LICENSE`、`THIRD_PARTY_NOTICES.txt`。
完全なPR変更一覧をページ分割して読み、renameは変更前のパスも判定する。取得漏れ・不正な一覧は拒否し、それ以外のパスはCI対象。
文書だけなら差分と必要な目視で確認し、CIの代わりの検証スクリプトや長い記録を作らない。
既存の軽いGitフックとPRの件名・Issue・検証記録の確認は維持する。
文書のみの merge は CI を起動しないが、PR 本文に `eng/conformance.py` の 0 violation の記録を要る（#648）。`merge-pr.py` は `conformance\.py` と `0 violation` が同じ行にあるかだけを見て、無ければ `--execute` を拒否する。plan は `conformanceRecorded` で事前に示す。

非文書変更では`check.yml`を`workflow_dispatch`で起動する。PR番号と期待headを渡し、workflow自身も取得したPRとcheckoutの一致を確認する。
統合道具は返されたrun ID、repository、workflow、イベント、PR番号、head、成功を照合し、実行前後のPRとrulesも比較する。
planは読み取りのみ。失敗・成否不明を自動再試行しない（`gh run watch`の中断は同じrunの待ち直し（最大3回）で、新しいrunは起こさない（#667））。待機が切れたら表示済みの`--ci-run`で同じrunを確認し、新しいCIを起動しない。
CIの中身は既存のGit規約・検証記録・差分空白の確認と木の conformance（`python eng/conformance.py`・#648）だけ。製品テストの成功結果を工程のために測り直さない。

## 強制と制限

全PR共通のrequired status checkは文書免除と両立しないので、main rulesetからこの条件だけを外す。
PR必須、squashのみ、force push・削除禁止、未解決レビュー拒否は維持する。
対象別CIの強制は正典の`eng/merge-pr.py`で行う。GitHub UIや直接の`gh pr merge`まで対象別に機械強制するものではない（planned）。
CI成功は製品動作や検証記録の真実性を保証しない。forkのheadを手動workflowのrefにできないため、統合道具は同repoの作業ブランチだけを受け付ける。

ADR 0021のCI頻度と過去の一律必須check記述を本決定が置き換える。差分検証・結果の再利用・各検査の判定基準は維持する。

## 検証

変更した統合道具とCI入口に限定して、文書の無起動、rename/混在変更、CI失敗・異なるheadの拒否、planの無変更を確認する。
このCI設定変更のPRをマージする際の手動実行を実接続の確認として再利用する。製品ビルド・製品テスト・全件ゲートは実行しない。
