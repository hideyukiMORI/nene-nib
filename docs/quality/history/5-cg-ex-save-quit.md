# 5-cg — Exの保存と終了（Issue #284・ADR 0066）

出典: docs/quality/gate-proofs.md の節 5-cg（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-03。FR-003 / FR-008 / D22に沿い、現在の文書の`:w` / `:q` / `:q!` / `:wq` / `:x`と省略名を追加。coreの名前表と閉じた要求を既存のEx評価から渡し、保存はGUIと共通の`save_document`、閉じる操作は既存の`CloseTab`。最後のタブの終了をuiの`deliver`末尾へ統一する。`:tabclose`やCtrl+F4の保存確認は維持する。

### 参照と限定

固定Vim 9.1を`-u NONE -i NONE -N -n -es`で非対話実行し、26命令×名前あり/無題・保存済み/未保存の4条件、計104ケースを観測した。`D:/NeNeNib/scripts/probe-ex-save-quit.py`、`out/probes/ex-save-quit-2026-10-03.json` / `.log`。`w` / `q` / `wq` / `x`系の省略、E32/E37、`:x`の変更時だけの保存を確認。専用文書以外への書き込み・前面化・クリップボード操作なし。ファイル名引数・範囲・追記・強制上書き・全タブ命令は本件の対象外。

### 自動検証

Windows 11・clang-cl 19.1.5・Debug（clang-tidy・ASan・UBSan）。共有保存とEx評価、タブ終了の直接依存に限定した。

| 確認する退行 | コマンド | 結果・記録 |
| --- | --- | --- |
| 型の追加・共有保存/終了の呼び出し・厳格な静的解析 | `. ./eng/toolchain.ps1; cmake --build build --target nib_tests NeNeNib` | 成功、`out/284-build02.log` |
| 省略と拒否、E32/E37、成功/失敗、文字コード/BOM/改行、undo/保存位置、他タブの保持、既存GUI保存 | `build/nib_tests.exe --ex-document` | 204 checks成功、`out/284-ex-document.log` |
| 新しい補完候補と設定の共有評価 | `build/nib_tests.exe --ex-settings` | 195 checks成功、`out/284-ex-settings-final.log` |
| `edit`と`exit`の候補衝突、ファイル一覧への要求 | `build/nib_tests.exe --ex-files` | 82 checks成功、`out/284-ex-files.log` |
| コマンド候補の順位と共通の入力/確定 | `build/nib_tests.exe --command-palette` | 356 checks成功、`out/284-command-palette.log` |
| 現在のタブを閉じた後の切替・dirtyと未読文書 | `build/nib_tests.exe --tabs` | 291 checks成功、`out/284-tabs.log` |
| 最後のタブを閉じた後のsessionの意味 | `build/nib_tests.exe --session` | 199 checks成功、`out/284-session.log` |
| 閉じるときだけのファイル履歴更新 | `build/nib_tests.exe --history` | 38 checks成功、`out/284-history.log` |
| core/applicationにファイルやOSの依存を増やさない | `python eng/symbols.py --build-dir build --require core application` | 2 libraries・違反0、`out/284-symbols.log` |
| 正典経路・型の配置・生成物 | `python eng/conformance.py --build-dir build` | 違反0、`out/284-conformance.log` |

計1365 checks。最初のビルドは`deliver`の認知的複雑度11が上限10を超えて停止（`out/284-build01.log`）。終了処理を`finish_tab_action`へ分けて修正し、規則は不変。最初の`--ex-settings`では補完末尾の旧期待値`buffers`が1件不一致だった。増えた名前表での正しい`exit`へテストを直し、`--target nib_tests`（`out/284-build03.log`）とこの195 checksだけ再実行した。他の6scopeの成功は関連入力不変なので再利用。差分C++の`clang-format --dry-run --Werror`と`git diff --check`も終了0。

自己レビュー: ARC-001 / ARC-004は名前表・保存関数・CloseTabの正典、ARC-003はsymbols、ARC-010 / CPP-002は閉じた要求と保存失敗、CPP-004 / CPP-011はoptional参照と1ファイル1型、QLT-001 / QLT-012は対象選択と再利用で確認。保存schema・port・adapterの変更なし。Waivers: none。

性能範囲: 起動・通常の文字挿入・一覧の照合/描画・ファイルI/Oの実装は不変。新しい保存/終了は明示したEx命令だけで実行され、通常打鍵のuiには終了のboolと条件分岐が加わるだけ。起動/巨大ファイル/5000候補等の全性能ベンチや全Vim再生は行わない。保存実装の抽出は既存保存の契約を新規scopeへ束ねて確認した。

製品・対象テスト・関連依存・環境が不変なら、文書・PR・mergeでは上記成功を再利用する。

### Releaseと保護対象の比較

`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`は`d078f9a92cff3450724e8d26cdc9a867855a20e4`で成功。`build/release-d078f9a/NeNeNib.exe`、1,296,896 bytes、SHA-256 `10F0B2D3C2EA6B946236F89514D20C45BDE9969FAF56EB634EF829837D5A8505`。configure 2.158s / build 136.882s。`out/284-release.log` / `out/release/d078f9a.json`に記録。この生成コマンドでは起動していない。

`python eng/protected-diff.py --base ecf4aa9 --head d078f9a`は終了0（`out/284-protected.log` / `out/protected/d078f9a.json`）。`fixtures 1853 -> 1853 / metadata 0 / deleted 0 / changed 0 / added 0`、`files changed none`、`scopes 32 / same 0 / 未測 31`、新規`--ex-document`も未測。比較用exeを渡していないため、全scopeの件数不変の証明にはしない。perf-reference / symbol-allowlist / conformance-rules / SettingsCodecの静的差分0。

文書追加後は`conformance.document_checks(root, paths, rules)`だけを呼び、規則IDと相対リンク・強制状態の一致を確認して違反0（`out/284-doc-conformance.log`）。C++と関連依存は不変なので製品の検査は再実行しない。

### 実機の限定確認

専用profileとalpha/beta/blockedの3文書を`D:/NeNeNib/evidence/ex-document-284/`へ準備。Release生成と[PR #285](https://github.com/hideyukiMORI/nene-nib/pull/285)のDraft作成後、18:51 JSTに今回分の前面確認を依頼し、hideが「今、実行してよい」と了承した。18:54〜18:55 JSTに`python D:/NeNeNib/scripts/ex-document-284.py --executable C:/Users/info/WORKS/NeNeNib/build/release-d078f9a/NeNeNib.exe`を1回実行して終了0。`out/284-window.log`、7枚のPNGと`record.json`は`out/frames-284/`。18:55:01 JSTに前面操作の終了をhideへ通知した。

サナが7枚すべてを読み、次を受理した。

- 未保存のbetaの`:q`はE37を一行表示し、本文`beta!`・未保存印・両方のタブが残る。ダイアログも書き込みもない。
- `:w`で`Written`と保存済みの印へ変わり、実ファイルが`beta!`+LFに一致する。
- 続いて`?`を足した`:wq`は`beta!?`+LFを書いてbetaだけを閉じ、alphaへ戻る。
- 無題の`:w`はE32で止まり、`:q`は変更のない無題を閉じる。変更した無題の`:q!`もそのタブだけを閉じ、alphaを保つ。
- 最後の変更のないalphaを`:x`で閉じて終了0。本文と更新時刻が不変で、書き込みを省くことを確認する。
- 読み取り専用にした専用のblockedへの`:wq`は`Could not write file`を表示し、`blocked!`と未保存印を残す。元のディスク内容は不変。`:q!`で終了0。確認後は専用ファイルの読み取り専用属性を戻した。
- 既存の`:tabclose`は未保存の確認を出し、取消で本文を残す。続く実キーCtrl+F4にも確認があり、「保存しない」で最後のタブを閉じて終了0。ディスク内容は元のまま。

3回のプロセス終了すべてが0。製品・対象テスト・関連依存は`d078f9a`から不変で、文書更新時の`git diff --exit-code d078f9a -- src tests eng CMakeLists.txt`も差分0。PRのReady/mergeでは1365 checks・Release・実機の成功結果を再利用する。実機のIME変換・クリップボード・全件性能はこの変更の確認に必要なく、実行していない。
