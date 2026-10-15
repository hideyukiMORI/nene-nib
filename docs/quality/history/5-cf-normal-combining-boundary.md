# 5-cf — 通常モードの結合文字境界（Issue #282・ADR 0065）

出典: docs/quality/gate-proofs.md の節 5-cf（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-03。D38の「結合文字から段階的に」を受け、通常モードの左右とDeleteはアクセント・濁点・共通のVSまでを一つとして扱う。Backspaceはmark一つ、VSは直前のcode pointと消す。上下・ページの着地は途中なら先頭へ寄せる。`CaretMoveRequest`で通常とVim INSERTの単位を区別し、既存の選択・replace・undoへつなぐ。本文・UTF-8・保存形式は不変。

### 参照と限定

非表示の自前RICHEDIT50W controlへメッセージを送り、25ケースを観測した。`out/probes/ordinary-characters-2026-10-03.json` / `.log`、使用DLLは`C:/Windows/System32/msftedit.dll` 10.0.26100.8875、SHA-256 `490d872e894935df2df5f7f2b9b9bc6b00b91e74412be2af55714b301ef3e3ef`。前面化・本物の鍵・クリップボードは使っていない。Notepadパッケージ内のDLLの直接ロードはAccessDeniedで使えず、この参照をNotepad本体の試験とは呼ばない。Unicodeの全書記素境界や複雑な絵文字・各言語固有のまとまりはD38の後続範囲。

### 自動検証

Windows 11・clang-cl 19.1.5のDebug（clang-tidy・ASan・UBSan）。全件は実行せず、直接変更する境界だけを選んだ。

| 確認する退行 | コマンド | 結果・記録 |
| --- | --- | --- |
| 新しい型と共有移動の全呼び出し元、厳格な静的解析 | `. ./eng/toolchain.ps1; cmake --build build --target nib_tests NeNeNib` | 成功、`out/282-build02.log` |
| 結合文字/VS/孤立mark/UTF-8/CRLF/長い行とpiece/上下着地/Shift選択/削除/undoとredo、既存移動、Vim INSERT・外部DeleteText | `build/nib_tests.exe --ordinary-characters` | 233 checks成功、`out/282-ordinary-characters-final.log` |
| Vimの既存文字境界・移動/削除・fixtureが変わらない | `build/nib_tests.exe --vim-characters` | 665 checks成功、`out/282-vim-characters.log` |
| core/applicationへOS依存を持ち込まない | `python eng/symbols.py --build-dir build --require core application` | 2 libraries・違反0、`out/282-symbols.log` |
| 正典経路・型の配置・生成物の保護 | `python eng/conformance.py --build-dir build` | 違反0、`out/282-conformance-final.log` |
| 差分C++の整形と空白 | `clang-format --dry-run --Werror <差分C++>; git diff --check` | 終了0 |

初回ビルドはテストの補助型`Bytes`を誤った名前空間で参照して失敗した（`out/282-build01.log`）。既存のテスト型へ修正し、製品のゲートは変えていない。231 checks成功後、自己レビューでVimの外部DeleteText維持を明示する2 checksを追加した。製品は不変のまま`--target nib_tests`だけをビルド（`out/282-build03.log`）し、変更した`--ordinary-characters`だけを再実行した。`--vim-characters`の成功は再利用。

再利用: 製品・対象テスト・関連依存・環境が不変の文書/PR/mergeでは上記成功を再利用する。通常入力の挿入・一覧の照合・起動・ファイルI/O・描画は変えていないため、その全性能ベンチと無関係なscopeは実行しない。新境界の長い行の契約は時間を測らず、結果と終端を確認する。前面の確認はReleaseと専用手順を準備し、hideの了承後に下記の限定手順を実施した。

自己レビュー: ARC-001 / ARC-004は文字境界と移動単位をcoreへ一本化、選択・replace・undoは既存経路。ARC-003はsymbols、CPP-002 / CPP-011は網羅switch・1ファイル1型、QLT-001 / QLT-012は対象と再利用根拠で確認。Waivers: none。schema変更なし。Release・保護対象比較・実機の結果は以下に記録する。

### Releaseと保護対象の比較

`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`は`f0efe72f476da2026219d7239f5015296a261c20`で成功。`build/release-f0efe72/NeNeNib.exe`、1,290,240 bytes、SHA-256 `445250FD4E42FF9E4FBCD67186357B5ABE0B211DBADBC39862F49CD2E289F058`。configure 2.218s / build 144.589s。`out/282-release.log` / `out/release/f0efe72.json`に記録した。この生成コマンドでは起動していない。

`python eng/protected-diff.py --base a787186 --head f0efe72`は終了0（`out/282-protected.log` / `out/protected/f0efe72.json`）。`fixtures 1853 -> 1853 / metadata 0 / deleted 0 / changed 0 / added 0`、`files changed none`、`scopes 31 / same 0 / 未測 30`、新規`--ordinary-characters`も未測。比較用exeを渡していないため、全scopeの件数不変の証明にはしない。`src/ui`・`src/adapters`・`eng`の静的差分も0。

### 実機の限定確認

専用profileと`combining.txt`を`D:/NeNeNib/evidence/ordinary-282/`に用意し、`D:/NeNeNib/scripts/ordinary-282.py --prepare`で作成した。今回分の前面確認を依頼し、hideが「やって」と了承した後、17:42〜17:43 JSTに`python D:/NeNeNib/scripts/ordinary-282.py --executable C:/Users/info/WORKS/NeNeNib/build/release-f0efe72/NeNeNib.exe`を1回実行。終了0、ログは`out/282-window.log`、9枚のPNGと`record.json`は`out/frames-282/`。17:43:06 JSTに前面操作の終了を知らせた。

サナが9枚すべてを読み、次を受理した。

- `e + U+0301`をRightでまとめて越え、表示は列3。Backspaceでaccentだけ外れた`ex`となり、undoで元の表示へ戻る。
- `か + U+3099`をShift+Rightで一緒に選択し、Delete後は`x`だけになる。
- `葛 + U+E0100`をDeleteで、`U+20000 + U+E01EF`をRight→Backspaceで削除すると、それぞれ`x`だけになる。
- `e + U+0301 + U+FE0F`のBackspaceはaccentとVSを除いて`ex`を残す。
- 5通りの削除後に保存し、全ファイルのUTF-8を期待値と照合してすべて一致。最後にundoで元の全バイト列へ戻して保存し、未保存印が消えたこと、終了コード0を確認した。

描画・IME実装は不変で、この確認ではIME変換自体を再実行していない。製品と対象テストは`f0efe72`から不変。以降の文書・PR・mergeでは898 checksとRelease・実機の成功結果を再利用する。PR #283へ同じ記録を載せる。
