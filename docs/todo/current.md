# いまのタスク — NeNe Nib

> GitHub Issue が正。ここは要約であり、Markdown のチェックリストをタスク状態として扱わない。
> 更新は実測でだけ行う。検証は差分から選び、関連入力が不変の成功結果を再利用する（QLT-001 / QLT-012・[ADR 0021](../adr/0021-diff-scoped-verification-and-result-reuse.md)）。
> Issue ごとの経緯は[日報](../reports/)、コマンドと数字は [gate-proofs](../quality/gate-proofs.md)。ここには書かない（Issue #124）。
> 再開地点は[2026-10-06の引き継ぎ](../handoffs/2026-10-06.md)。#291は字体選択・可視字形・幅索引まで実装し、長行の単打と連続入力を大幅に短縮した。通常版とsplitの固定90本、長行単打各240本、表示/IME/対象契約を検証済み。
> 長行の安定条件とsplit総時間2msが未達で、PR #293はdraft、main統合とsplit採用は保留。[gate-proofs 5-cm](../quality/gate-proofs.md#5-cm--字体選択可視字形表示幅とsplit再評価issue-291adr-007100730074)。

## 運用（2026-10-03）

- 既定は現在のサナの単体実行。hide が現在の作業で分担を明示した範囲では、その指定を優先する（AGENTS.md）。2026-10-03 の #272 の仕上げは、hide の指示で設計サナが設計・判断・受理、SOL の実装サナが調査・計測器・CI を担当した。この分担を次の作業の既定にはしない。
- 繰り返す手順は `eng/` のスクリプト、道具の出力は `out/`。依頼書の型は [`docs/templates/implementation-seat-brief.md`](../templates/implementation-seat-brief.md)。過去の ADR 0038 / 0039 にある固定モデルや一律の分担より、現在の AGENTS.md と hide の指示を優先する。

## いまの Issue と次の順

2026-10-06追記: #291から分離した#294幅索引（[PR #296](https://github.com/hideyukiMORI/nene-nib/pull/296)）と#295ステータス（[PR #297](https://github.com/hideyukiMORI/nene-nib/pull/297)）も独立構成の比較で保留。両方Draft、main未変更。[最新引き継ぎ](../handoffs/2026-10-06.md)。

| 順 | Issue | 状態 |
| --- | --- | --- |
| 1 | #264 Ctrl+P の面で日本語入力を受け、名前の照合をコードポイントの境目で行う（ADR 0061・施主決定 D31・D32） | **済み**・工程 1（照合と IME の構えと変換の行き先）と工程 2（ui の IME の開閉と入力行の描画と候補窓）と差し戻し 2 件を受理し、実機は IME の開閉と変換（差し戻し 2 の後の撮り直しを含む）と速さの 6 本を確かめた。検証の記録は [gate-proofs 5-bz](../quality/gate-proofs.md) |
| 2 | #272 同じフォルダ（3/5・`/`・ADR 0062・D33〜D35） | **済み**。[PR #277](https://github.com/hideyukiMORI/nene-nib/pull/277) を統合。検証と再利用の根拠は [gate-proofs 5-cc](../quality/gate-proofs.md#5-cc--ctrlp-の同じフォルダissue-272adr-0062) |
| 3 | #278 ブックマーク（4/5・`*`・通常 Ctrl+D / Vim Ctrl+Shift+D・ADR 0063・D36・D37） | **済み**・明示登録・永続化・一覧からの付け外し・消えた登録の保持。実装と対象自動検証・実機を受理し、[PR #279](https://github.com/hideyukiMORI/nene-nib/pull/279)で統合。続いて #280 の Vim の入口へ |
| 4 | #280 Vim の `:e` `:b` `:ls`（5/5・ADR 0064） | **済み**・共通一覧への接続・引数の検索・省略名。実装と対象自動検証・実機9場面を受理。[PR #281](https://github.com/hideyukiMORI/nene-nib/pull/281)、記録は [gate-proofs 5-ce](../quality/gate-proofs.md#5-ce--exから共通のファイル一覧issue-280adr-0064)。Ctrl+P統合の5本が揃った |
| 5 | #282 通常モードの矢印・Backspace・Delete と結合文字（D38・ADR 0065） | **済み**。アクセント・濁点・共通の異体字セレクタから段階的に対応。対象898 checks・Release・実機9場面/保存5通りを受理。[PR #283](https://github.com/hideyukiMORI/nene-nib/pull/283)、[gate-proofs 5-cf](../quality/gate-proofs.md#5-cf--通常モードの結合文字境界issue-282adr-0065) |
| 6 | #284 基本Exの保存と終了（ADR 0066） | **実装・検証済み**。対象1365 checks・Release・実機7場面/保存内容/3回の正常終了を受理。[PR #285](https://github.com/hideyukiMORI/nene-nib/pull/285)、[gate-proofs 5-cg](../quality/gate-proofs.md#5-cg--exの保存と終了issue-284adr-0066) |
| 7 | #286 Exのファイル名付き保存とsaveas（ADR 0067・D39） | **実装・検証済み**。対象991 checks・Release・実機8場面/保存内容/3回の正常終了を受理。名前は保存成功時だけ更新。[PR #287](https://github.com/hideyukiMORI/nene-nib/pull/287)。[gate-proofs 5-ch](../quality/gate-proofs.md#5-ch--名前付きex保存issue-286adr-0067) |
| 8 | #288 split前の区切り / #290 範囲・判定条件・限定描画試作（D40・ADR 0068） | **下準備・試作済み、製品採用は保留**。注釈付き`checkpoint/pre-split-20261003`を保存。長い日本語行で左右分割の費用が増え、事前条件を満たさない。[準備資料](../design/2026-10-03-split-preparation.md) / [gate-proofs 5-cj](../quality/gate-proofs.md#5-cj--splitの下準備と限定した描画費用issue-290adr-0068) |
| 9 | #291 長い日本語行の単画面の入力遅延（ADR 0069〜0074、0072は実験不採用） | **字体選択・可視字形・幅索引まで実装・対象検証済み**。長行を大幅に短縮したが安定条件で総合hold。PR #293はdraft、mergeとsplit採用は保留。[gate-proofs 5-cm](../quality/gate-proofs.md#5-cm--字体選択可視字形表示幅とsplit再評価issue-291adr-007100730074) |
| 10 | その他の候補: 一般Exの残り / 複雑な書記素境界 | 未起票 |

2026-10-02 に統合（11 回目の区切り）: #264 Ctrl+P の面の日本語入力（ADR 0061・施主決定 D31・D32・PR #269）・#270 面は絞り込みの結果を持ち frame には見えている行だけ（ADR 0062・施主決定 D33・D34 も仕様へ・PR #273）・#271 裏のワーカー 1 本とフォルダの列挙（ADR 0062・PR #274）。

2026-09-30 に統合（8 回目の区切り）: #258 Ctrl+P を開くと開いているタブの一覧が出て行頭の記号で出どころを絞る・#259 閉じたファイルを履歴に覚え Ctrl+P の一覧から開く（どちらも ADR 0060・施主決定 D28〜D30）。Ctrl+P の統合（FR-006）は 5 本のうち 2 本が main に入った。

2026-09-30 に統合（7 回目の区切り）: #252 窓を閉じるときに開いていたタブを `session.v1` に覚える・#253 ファイルを指定せずに起動したとき前回のタブを戻し見るときに読む（どちらも ADR 0059・施主決定 D24〜D27）。前回のタブの復元（FR-009）は main に入った。

2026-09-30 に統合（6 回目の区切りまで）: #238 タブの帯とマウス（ADR 0056 の決定 8・9・12）・#239 タブの鍵（決定 10）・#240 タブの一覧と Ex の `tabnext` と Vim の `gt` `gT`（ADR 0057）・#248 Ctrl+Tab は最近使った順（ADR 0058・施主決定 D23）。#238 と #239 の merge は 09-29 の夜。複数タブの 4 本の縦切りは全部 main に入った。

2026-09-29 に統合: #204 数字レジスタ `"0`〜`"9` と小削除 `"-`（ADR 0050）・#210 クリップボードのレジスタ `"+` `"*`（ADR 0051）・#209 `.` の記録は待ちの状態の数字を残す・#205 改行を含む文字単位の `p` `P` のキャレット・#206 NORMAL の `X`・#208 `u` と Ctrl-r の後のキャレット（ADR 0052）・#216 Vim の 1 文字と結合文字（ADR 0053）・#222 `W` `E` `B` `ge` `gE`・#224 後ろ向きの語の移動が本文の先頭に当たる形・#226 前向きの語の移動の失敗の印・#229 fixture の鍵の記法の表の 1 本化（ADR 0054）・#235 貼り付けた本文の改行を文書の形に揃える（ADR 0055・D19）・#230 fixture の再生は `:normal!` と同じ打ち切り・#237 複数タブの状態（ADR 0056・D20〜D22）。候補（未起票）: 巨大な削除の後の打鍵のベンチとレジスタの本文の共有・oracle に undo の塊を区切る記法。

2026-09-23 に統合: #124・#131・#141・#130・#144・#146（usage の集計）・#117（`^M` の描画・ADR 0040）・#151・#147（C1 の 4 桁）・#152・#148（incsearch・ADR 0041・既定オンは施主決定 D18）・#162・#160（単体テストの分割・ADR 0042）・#165・#140（`assert_uncovered`・`measure`）・#168（Ctrl-G / Ctrl-T・ADR 0043）・#172（D18）・#175 Tab の tab stop（ADR 0045）・#174 add の chunk 化（ADR 0044）・#176 マクロ `q` `@`（ADR 0046）・#180 `recording @a`・#179 16 MiB の打鍵ベンチ・#184 改行の索引の共有（ADR 0047）・#190 oracle の `q` の拒否を狭める・#191 `erase().insert()` を `replaced` に・#193 名前つきレジスタ `"a`（ADR 0048）・#198 `VimKeyTable` の切り出し・#200 `<Space>` `<BS>`（ADR 0049）。

splitの下準備はD40でhide了承済み。本実装は試作結果から保留。その他の候補の順は未確認。open の一覧は `gh issue list --state open` が正。

## いまの数字（2026-10-03・差分ごとの検証済み実装）

| 項目 | 値 | 正本 |
| --- | --- | --- |
| Vim fixture | 1853 件（`undo-caret-*` 109 件・`combining-*` 92 件を含む・`macro-*` 20 件は `register` 欄で再生だけ・`register-*` は数字と小削除の 82 件を含む・`space-*`。`"+` `"*` は fixture にできず契約） | `tests/vim/VimFixtures.hpp` の 5 行目（CNF-010） |
| 既定の `nib_tests` | scope 33。全件checksは最新差分で未測（過去の全件実測19449 checks）。`--bookmarks` 40 / `--ex-files` 82 / `--ordinary-characters` 233 / `--ex-document` 204 / `--ex-write-path` 90 checksを対象実行で確認（`--vim-clipboard` `--vim-characters` `--tabs` が 2026-09-29・`--session` `--history` が 2026-09-30・`--background-work` が 2026-10-02 に新規・1 scope = 1 翻訳単位・表は `NibTests.cpp`・ADR 0042。前回のタブの一覧の adapter は CTest `nib_sessions`・#252。閉じたファイルの履歴の adapter は CTest `nib_histories`・#259。ワーカーとフォルダの列挙の adapter は CTest `nib_folders`・#271） | [gate-proofs 5-bd 〜 5-ch](../quality/gate-proofs.md)。前後比較は `python eng/protected-diff.py --base <ref> --build`（#130・`out/protected/<短い SHA>.json`） |
| ADR | mainは0068まで。#291の作業枝は0074まで（0072は実験不採用）。#294は0074、#295は0070を独立構成へ分離し性能受理を保留。split製品採用も保留 | [`docs/adr/README.md`](../adr/README.md) |
| 見た目の確認 | `python eng/verify-window.py [--open <file>] [--vim] --capture <dir> --keys "<鍵>"` → PNG を Read で見る・`eng/compare-frames.py --regions --expect`。撮影は同じ機械で 1 席ずつ（覆われると `covered` で終了 1・#140） | #131・[gate-proofs 5-al](../quality/gate-proofs.md) |
| 実機用 Release | `pwsh -NoProfile -File eng/build-release.ps1 -Ref main` → `build/release-<短い SHA>/NeNeNib.exe` と `out/release/<短い SHA>.json`（起動は設計席） | [ADR 0038](../adr/0038-model-per-seat-and-scripted-preparation.md) 決定 5・#129 |
| 席の消費 | `python eng/usage-report.py --since <日付>` → 席ごとの turns・最大文脈・cache_read・seat_tokens | #146・[gate-proofs 5-an](../quality/gate-proofs.md) |
| 速さ（実機） | 基準値は 起動 191 ms・窓 35 ms・1 打鍵 0.9 ms（空の文書）・16 MiB を開く 250 ms・16 MiB で 200 打鍵 7.4 ms（6 本目・#179 / #184）。2026-10-03 の #272 は同じフォルダ5000件の入力2.630 msを新規採用し、通常入力1.016 msは基準内。起動・大容量は不変なので #271 の成功を再利用（詳細は gate-proofs 5-cc）。タブ 20 本の一覧で起動した時間は 1 本のときと同じ（手で測る・[gate-proofs 5-bw](../quality/gate-proofs.md)）。計測は窓を最前面に出すので施主に確かめてから回す | `eng/perf-reference.json`（ADR 0016） |

既知の既存失敗: `eng/test-conformance.py` の `test_verification_policy.py` の一部は cp932 の端末で pwsh の出力が読めず落ちることがある（道具側は #106 で直した。残れば別 Issue）。

## 動かないもの

64 MiB 超のファイル・文字コードと改行の手動切り替え・IME の再変換と TSF 固有の機能・
VISUAL の `p u ~ > < J I A gv` と `X D C Y`・ドラッグで VISUAL・矩形の `c I A C > < J ~` と VISUAL の中の `p`・`virtualedit`・
autoindent・読み取り専用のレジスタ（`".` `":` `"/` `"%`）と `"=`・`q"`・矩形の種類を保った `"+` の往復・矩形レジスタへの `"A` の追記・通常モードの複雑な絵文字・言語固有の書記素境界（アクセント・濁点・共通VSは#282で段階対応）・行頭の孤立した結合文字の幅・INSERT の中の矢印による undo の区切り・`whichwrap` の設定・マクロの中の Ex と録画中の表示・`J s S R`・r の制御文字・Ctrl-e/y・検索の `:s` `:g`・履歴・offset・`\v` `\c` `\(` `\|` `\{`・`ignorecase`・
テキストオブジェクト `it ip is`・一般 Exの残り（ファイル名展開、範囲、パイプ、履歴、強制上書き、全タブ終了）・
タブのドラッグの並べ替え・窓の位置と大きさの復元・強制終了したときのタブの一覧・Ctrl+1〜9・Ctrl+Tab で歩いている間の一覧の面・`g<Tab>` と `:tabfirst` `:tablast` と `:tabnext +N`・一覧の種別のアイコンと履歴の時刻・Markdown プレビュー・折り返し・横スクロール・ドラッグ選択。

## 段階

| 段階 | 状態 |
| --- | --- |
| Phase 0 言語の実測 | ✅ 2026-09-15（114 記録・`docs/quality/phase0-results.json`） |
| Phase 1 文書とゲートの足場 | ✅ 2026-09-15（Issue #1・PR #2） |
| Phase 2 negative proof | ✅ 2026-09-15（26 反例・`docs/quality/gate-proofs.md`） |
| Phase 3 縦切り | 🔲 進行中。#3 窓（ADR 0007）✅ → #5 見た目（ADR 0008）✅ → #7 編集（ADR 0009）✅ → #11 ファイル（ADR 0010）✅ → #16 速さ（ADR 0011）✅ → #19 起動の内訳 ✅ → #22 Vim の最初の縦切り（ADR 0012）✅ → #24 窓を先に見せる（ADR 0013）✅ → #28 IME（ADR 0014）✅ → #31 タブの帯（D16）✅ → #30 計測器の揺れ ✅ → #36 欠測の言い方 ✅ → #44 生成物の SHA（CNF-010）✅ → #43 Vim の 2 本目（ADR 0015）✅ → #47 CI の速さの基準値（ADR 0016）✅ → #52 カラーテーマ C1（ADR 0017）✅ → #53 VISUAL（ADR 0018）✅ → #58 画面移動 ✅ → #60〜#70 設定と C2〜C4b ✅ → #72 / #76 / #79 / #84 / #87 / #93 / #100 / #91 / #108 / #112 / #85 / #123 Vim の縦切り ✅（ADR 0026〜0037）→ **#129 / #130 / #131 下ごしらえのスクリプト化（ADR 0038）進行中** |
| Phase 4 公開 | 🔲 |
