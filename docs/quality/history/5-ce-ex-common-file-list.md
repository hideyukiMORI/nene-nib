# 5-ce — Exから共通のファイル一覧（Issue #280・ADR 0064）

出典: docs/quality/gate-proofs.md の節 5-ce（2026-10-16 に D50 で移動・本文は移動時のまま）

### 対象・依存・退行の根拠

FR-006 / D5 / D28の最後の入口を実装。`ExResult`の型付き一覧要求→`EditorController::run_palette_request`→既存`open_palette`を使う。`:e`は全候補、`:b` / `:ls`はタブ、引数は検索欄。`palette_marks`の逆引きも同じ表を使い、全候補の検索で先頭の記号を文字として保持する。候補列・照合器・読み込み・描画・IME処理・port・保存形式は変更しない。

固定した手元のVim 9.1（2024-01-03 build）の非対話probeを36ケース実行した。`python D:/NeNeNib/scripts/probe-ex-files.py`、`out/probes/ex-files-2026-10-03.json`。`e`〜`edit`、`b`〜`buffer`、`ls` / `files` / `buffers`の綴りを採用し、`fi` / `file` / `l` / `lis`が別命令であることを確認。Vimの直接open・reload・番号切替はFR-006の一覧を優先して実装しない。

### 自動検証

Windows 11・clang-cl 19.1.5の既存Debug環境（clang-tidy・ASan・UBSan）。各scopeは`build/nib_tests.exe <scope>`で実行した。

| 確認する退行 | コマンド | 結果・記録 |
| --- | --- | --- |
| 新しい公開型・controllerとの接続・静的規則 | `. ./eng/toolchain.ps1; cmake --build build --target nib_tests NeNeNib` | 成功、`out/280-build02.log`。テストの期待値修正後は`--target nib_tests`のみ再ビルド、`out/280-build03.log` |
| Exの名前・引数・拒否・256 bytes・記号と日本語・出どころ・選択・本文/undo/register・遅延open | `--ex-files` | 82 checks成功、`out/280-ex-files.log` |
| 共有Ex評価器と補完が既存設定を壊さない | `--ex-settings` | 195 checks成功、`out/280-ex-settings.log` |
| OpenTabListの共通化と既存Exタブ命令 | `--tabs` | 291 checks成功、`out/280-tabs.log` |
| 候補追加の順・共通入力・IME・照合の境界 | `--command-palette` | 356 checks成功、`out/280-command-palette-fixed.log` |
| モジュール境界・型・規約 | `python eng/conformance.py --build-dir build` | 0 violations、`out/280-conformance.log` |
| core/applicationにOS依存を足さない | `python eng/symbols.py --build-dir build --require core application` | 2 libraries / 0 violations、`out/280-symbols.log` |
| 変更C++の整形 | `clang-format --dry-run --Werror <変更C++>` | 終了0 |

初回ビルドでは新しいテストのoptional参照を静的解析が検出し、確認してから読む形へ修正した（`out/280-build01.log`）。初回`--command-palette`ではサナが書いた同点時の期待順が1件失敗。既存の設定候補は文字列順であることをコードで確認し、期待値を`ls, edit, tabs`へ直した。製品の順は変えていない。成功した他のscopeは再実行していない。

再利用: #278の5000候補ベンチ（5有効試行・中央値2.708ms、`out/speed/2026-10-03T06-38-44Z.json`）を再利用する。ファイル候補の生成・`listed_positions`・`CommandPalette`・描画・adapter・計測器・環境は同一。新しい逆変換は一覧を開く時だけで、打鍵区間へ入らない。Exの設定候補は5件増えたが全ファイルの照合対象には含まれない。全件Vim再生・全性能ベンチは不要。成功済みの実装・対象テスト・関連依存を変えない文書/PR/mergeの工程では再測定しない。

規則: ARC-001 / ARC-003 / ARC-004 / ARC-010 / ARC-012 / CPP-002 / CPP-004 / CPP-011 / CPP-012 / QLT-001 / QLT-012 / QLT-013。Waivers: none。Vimの互換範囲はADR 0064に明記。実機とRelease・保護対象の比較は以下に記録する。

### Releaseと保護対象の比較

`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` は実装`a0d4d0b9aa92aa5a880df8664f741fc6779a0075`で成功。`build/release-a0d4d0b/NeNeNib.exe`、1,288,192 bytes、SHA-256 `543C7BAF31024C8BCB7E045663B896CEEB49BA1CF2C9A67D2D1E2C020443ACE1`。configure 2.632s / build 148.375s。記録は`out/280-release.log` / `out/release/a0d4d0b.json`。この生成コマンドでは起動していない。

`python eng/protected-diff.py --base 4a13e4c --head a0d4d0b`は終了0（`out/280-protected.log` / `out/protected/a0d4d0b.json`）。実出力: `fixtures 1853 -> 1853 / metadata 0 / deleted 0 / changed 0 / added 0`、`files changed none`、`scopes 30 / same 0 / 未測 29`、新規`--ex-files`も未測。比較用exeを渡していないため、既存scope全件の件数不変の証明にはしない。perf-reference / symbol-allowlist / conformance-rules / SettingsCodecの静的差分は0。

### 実機の限定確認

専用profileと9場面の確認手順を準備して前面確認を依頼し、hideが2026-10-03 16:47 JSTに了承。`python D:/NeNeNib/scripts/ex-files-280.py --executable C:/Users/info/WORKS/NeNeNib/build/release-a0d4d0b/NeNeNib.exe`を1回実行し、終了0。ログは`out/280-window.log`、9枚のPNGと`record.json`は`out/frames-280/`。専用文書・profileは`D:/NeNeNib/evidence/ex-files-280/`。

サナが9枚すべてを読み、次を受理した。

- `:e`で空の検索欄と4候補が出る。タブと同じフォルダが同じ面に並ぶ。
- `:b alpha`は`#alpha`に1件へ絞り、確定前はbetaの本文のまま。Enter後は既存のalphaタブに移る。
- `:ls`はタブ2件だけで現在のalphaが選ばれ、未保存の印を表示する。betaへ移ってから`:buffer alpha`で戻っても`alpha body!`とカーソルが残る。
- `:e #日本語 memo`は先頭記号を検索文字として保持し、同じフォルダの1件が残る。Enterで新しいタブが開き、日本語本文が読める。
- `:edit!`は`Not supported: edit!`の1行。`:ls absent`は候補なし（0/0）で、Enterしても現在の文書を変えない。
- 終了前にalphaの編集をundoし、終了コード0とディスク上の元の本文をスクリプトが照合した。16:48 JSTにhideへ前面操作の終了を通知。

この確認ではIME変換そのものを再実行していない。日本語は共有入力へUTF-16の文字メッセージとして渡したもので、IMEの構えは`--ex-files`と`--command-palette`の対象契約、既存のUI/IME実装は不変性で確認する。

自己レビュー: ARC-001 / ARC-004は同じ表・評価器・一覧・open経路、ARC-003はsymbols、CPP-002 / CPP-004 / CPP-011は閉じた選択肢・安全なoptional参照・1ファイル1型、QLT-001 / QLT-012は対象指定と成功結果の再利用で確認。PR #281へ同じ検証を記録する。製品と対象テストは実測した`a0d4d0b`から変更していない。以降の文書・Ready・mergeだけを理由に製品テストを再実行しない。
