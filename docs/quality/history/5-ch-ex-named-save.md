# 5-ch — 名前付きEx保存（Issue #286・ADR 0067）

出典: docs/quality/gate-proofs.md の節 5-ch（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-03。FR-003 / FR-008 / D8 / D22 / D39。`:w 名前` / `:wq 名前` / `:x 名前`と`:sav`〜`:saveas 名前`を既存保存へ接続。名前ありの別名writeはコピーで保存位置と名前を保ち、無題への保存とsaveasは成功時だけ命名する。hideは19:39 JSTに「良い。進めて。」とD39の推奨案を了承した。失敗時にも名前を変えるVimの一部の挙動とは意図的に異なる。

### 参照と検証範囲

固定Vim 9.1を`-u NONE -i NONE -N -n -es`で12命令×名前あり/無題・変更あり/なしの4条件、計48ケース観測した。`D:/NeNeNib/scripts/probe-ex-write-path.py`と`out/probes/ex-write-path-2026-10-03.json` / `.log`。コピー後のdirty、E13/E37、saveas、空白、失敗時の名前を確認。前面操作なし。範囲・強制保存・展開・追記・全タブ終了は対象外。

### 自動検証

Windows 11・clang-cl 19.1.5・Debug（clang-tidy / ASan / UBSan）。保存要求の呼び出し元と新規作成の境界へ限定した。

| 確認する退行 | コマンド | 結果・記録 |
| --- | --- | --- |
| FilePortの共有呼び出しと型・静的解析 | `. ./eng/toolchain.ps1; cmake --build build --target nib_adapter_tests nib_tests NeNeNib`、後続は`nib_tests NeNeNib` / `nib_tests`だけ | 成功、`out/286-build02.log`・`04.log`〜`07.log` |
| 新規作成/既存拒否、他者の一時ファイル保護、失敗した置換後の元ファイルと一時ファイル、既存形式・パス | `build`から`nib_adapter_tests.exe --files` | 64 checks・0 failures、`out/286-files.log` |
| 名前/空白/Windowsパスと拒否、コピーと命名、D39と変換失敗、undo/保存位置、終了条件、他タブ保護 | `build/nib_tests.exe --ex-write-path` | 90 checks成功、`out/286-ex-write-path-final.log` |
| 引数なしの既存Ex保存・終了とGUIの共通保存 | `build/nib_tests.exe --ex-document` | 204 checks成功、`out/286-ex-document.log` |
| 新候補saveasと共有設定評価 | `build/nib_tests.exe --ex-settings` | 195 checks成功、`out/286-ex-settings.log` |
| Ex解析の名前追加と既存のファイル一覧要求 | `build/nib_tests.exe --ex-files` | 82 checks成功、`out/286-ex-files.log` |
| 共通入力/確定と補完の順位 | `build/nib_tests.exe --command-palette` | 356 checks成功、`out/286-command-palette.log` |
| OS境界と未宣言の外部依存 | `python eng/symbols.py --build-dir build --require core application` | 2 libraries・違反0、`out/286-symbols-final.log` |
| 正典・型配置・依存・文書 | `python eng/conformance.py --build-dir build` | 違反0、`out/286-conformance.log`（文書追加後の記録は末尾へ） |

計991 checks。最初の基盤のEx204（`out/286-ex-document-baseline.log`）はcontroller実装後に再実行したため二重計数しない。adapter64はadapter/port/対象試験を変更していないため再利用。後半の修正は新規のファイル名の禁止文字判定と新規scopeのテスト準備のみで、引数なしEx・設定/候補・既存ファイル一覧の意味と対象試験は不変。837 checksを再利用する。タブの保存/同一パスの新しい呼び出しは新規scope内で確認し、既存CloseTab/session/historyは変更していないため#284の成功結果を再利用する。

初回の機能ビルド（`out/286-build03.log`）はテストのoptional未検査参照と認知的複雑度11（上限10）で停止。参照を束縛してガードを認識できる形にし、失敗1ケースの準備を関数へ分けた。規則は不変。初回の新規scopeは2/90失敗（`out/286-ex-write-path.log`）。表示高さの指定漏れと、行を`|`で連結する補助関数でLFを期待したテスト側の誤りを直した。本文は`vim_body`で評価する。symbolsは`string_view::find_first_of`由来の未宣言`memchr`を1件検出（`out/286-symbols.log`）。新しい文字判定を純粋な文字比較へ変更し、許可リストを変更せず違反0になった。修正したscopeとsymbolsだけを再実行した。

自己レビュー: ARC-001 / ARC-004は共通`save_document` / `FilePort::write` / `CloseTab`、ARC-003 / ARC-007 / ARC-009はresolveとOS APIのadapter境界、ARC-010 / CPP-002は閉じた要求と失敗、CPP-004 / CPP-011はoptionalと1ファイル1型、QLT-001 / QLT-012は差分に対応する範囲と再利用、QLT-013はReleaseと前面操作の分離で確認。UIの変更は失敗enumの網羅だけ。保存schema・性能基準・waiverに変更なし。

性能範囲: 新しい処理は明示したEx保存時だけで、通常打鍵・描画・一覧照合・起動の経路は不変。既存replaceのI/O列は維持し、新規作成モードは存在確認と上書きしない最終配置を使う。全性能/全Vimを再実行する根拠はない。Releaseと専用ファイルの実機確認を別途記録する。

差分C++の`clang-format --dry-run --Werror`、`git diff --check`、文書追加後の`python eng/conformance.py --build-dir build`が成功（違反0、`out/286-conformance-final.log`）。

### Releaseと保護対象の比較

`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`はcleanな`591b99fa7bbd2b6a4149fe45b81c81cafeb3e03a`で成功。`build/release-591b99f/NeNeNib.exe`、1,302,528 bytes、SHA-256 `312B55BE0A62F8593CEB95AF1EDC87EB13C3BB7C7E39A739948A43DF939C66E6`。configure 2.148s / build 138.892s。`out/286-release.log` / `out/release/591b99f.json`。生成だけで起動はしていない。

`python eng/protected-diff.py --base 12a0a83 --head 591b99f`は終了0（`out/286-protected.log` / `out/protected/591b99f.json`）。`fixtures 1853 -> 1853 / metadata 0 / deleted 0 / changed 0 / added 0`、`files changed none`、`scopes 33 / same 0 / 未測 32`、新規`--ex-write-path`も比較器では未測。比較用exeを渡していないため全scopeの件数不変の証明にはしない。perf-reference / symbol-allowlist / conformance-rules / SettingsCodecの静的差分0。

[PR #287](https://github.com/hideyukiMORI/nene-nib/pull/287)をDraftで作成。専用profileと文書、8場面の手順は`D:/NeNeNib/scripts/ex-write-path-286.py`、専用文書は`D:/NeNeNib/evidence/ex-write-path-286/`。`--prepare`は成功し、前面操作は行っていない。20:02 JSTに今回分の前面確認を依頼した。実機結果は未確認で、了承後に同じReleaseを確認して記録する。

### 実機の確認

hideの「やって」を受け、20:09 JSTに`python D:/NeNeNib/scripts/ex-write-path-286.py --executable C:/Users/info/WORKS/NeNeNib/build/release-591b99f/NeNeNib.exe`を実行。名前あり文書の5場面は成功し、`out/frames-286/01`〜`05`のPNGをすべて目視で受理した。コピー後の名前/未保存印、E13で既存ファイル保護、コピーを作ったwqのE37、失敗したsaveasの旧名保持、相対の日本語名でのsaveas成功を確認。alphaの元バイト列、コピー2件の全バイト列、既存ファイルの不変も照合した。続く変更のないxは指定先を作らず、保存先の更新時刻も変えず終了0。

初回は無題文書のタイトル比較で停止して終了1（`out/286-window.log`）。入力をPostMessageで送った直後、処理完了前にもタイトルを読む手順だったため、未保存の無題タイトルへの反映を待ってから比較するよう外部スクリプトを修正した。製品コードは変更していない。20:09:40 JSTに前面操作を終了したと通知し、20:11 JSTに残り3場面だけの再開を依頼した。成功した5場面と正常終了1回は保持し、再実行しない。タイトル保持・無題の命名/同名保存終了・他タブ保護の残りは未確認。


hideの再開指示「やって」を受け、20:18〜20:19 JSTに残りだけを`python D:/NeNeNib/scripts/ex-write-path-286.py --remaining --executable C:/Users/info/WORKS/NeNeNib/build/release-591b99f/NeNeNib.exe`で実行して終了0（`out/286-window-remaining.log`）。処理後に比較した無題のタイトルは、保存失敗前後とも`● 無題 - NeNe Nib`で一致した。無題へ名前を付けて`draft`を書き、同じパスへのwqで`draft!`を書いて終了0。別の開いているタブへのsaveasはE139で拒否し、両文書の全バイト列を保持した。両タブは通常のqで閉じ、終了0。一時ファイルの残りもない。

20:19:15 JSTに前面操作終了をhideへ通知。サナが残りの06〜08を含む8枚すべてを読み、メッセージ・本文・名前・未保存印を受理した。正常終了は計3回（初回の中断した無題プロセスはこの数に含めない）。画像と`record.json`は`out/frames-286/`。初回に完了した5場面とcopy/saveas/clean-xの結果は保存して再利用し、残り3場面だけを再実行した。製品の修正・Release再生成・成功済みの自動テスト再実行は不要だった。

製品・対象テスト・関連依存は591b99fから不変。文書追加時の`git diff --exit-code 591b99f -- src tests eng CMakeLists.txt`が差分0。文書だけは`conformance.document_checks`で確認し、違反0（`out/286-doc-conformance-final.log`）。Ready/mergeでは991 checks・Release・8場面の成功を再利用する。実機IME・クリップボード・全件性能は本件の変更に必要なく実行していない。
