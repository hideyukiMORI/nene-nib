# いまのタスク — NeNe Nib

> GitHub Issue が正。ここは要約であり、Markdown のチェックリストをタスク状態として扱わない。
> 更新は実測でだけ行う。検証は差分から選び、関連入力が不変の成功結果を再利用する（QLT-001 / QLT-012・[ADR 0021](../adr/0021-diff-scoped-verification-and-result-reuse.md)）。
> 2026-10-10 22:36 JST: #380の文書表示値移管は固定2利益/2費用、16300checks、新4probe、GUI8対0px、正式singleが成立。独立最終レビューでP0/P1/P2なし、技術受理。通常PR/CI/main/収載/整理後もhideの停止まで継続（gate-proofs 5-dq）。#378はPR379で統合/収載/整理済み。#365/#373は未解決。
> 21:55 JST時点の履歴: #378の検索preview位置変換を技術受理。固定2利益/5費用、224checks、新7probe、12画面0px、正式2指標が成立。通常PR/CI/main/収載/整理後もhideの停止まで改善継続（gate-proofs 5-dp）。#376はPR377で統合/収載/整理済み。#365/#373は未解決。
> 21:05 JST時点の履歴: #376の確定検索共有を技術受理。固定3利益/5費用、2002checks、新8probe、原13＋補足2のGUI、正式8指標が成立。通常PR/CI/main統合/恒久収載/整理後もhideの停止まで改善継続（gate-proofs 5-do）。#367はPR375で統合/収載/整理済み。#365/#373は未解決。
> 19:54 JST時点の履歴: #367の型付きdispatch設計・reserve一挿入・対象検証・固定比較が完了。30/120行の局所利益、空短費用、7画面0px、正式8指標成功を確認。独立レビューで技術受理、通常PR/CI/収載/整理後もhideの停止まで改善を続ける（gate-proofs 5-dn）。#372は保留記録をPR374で統合/収載/整理済み。#365/#373は未解決。
> 18:26 JST時点の再開指示（履歴）: hideの追記指示により、次回は#364の独立再実験と結果別対応 → #367のdispatch設計からの再開 → 当日の継続改善の順に進める（#370）。最新の実施指示は[引き継ぎ冒頭](../handoffs/2026-10-10.md)。この指示保存では実験を開始していない。旧#364の不採用記録は維持し、再評価結果と分ける。
> 18:04 JST時点の停止記録（#368）: #367は先行ADR0100と未完成の計測器草案まで。dispatchの網羅性と関数長上限を両立する設計が未決、製品/build/GUI/速度測定は未実行。D作業木・出力・briefとSHA照合済みsnapshotを保持した。その後の再開指示は上記#370を優先する。
> 第三陣は#351 / PR #352と#353 / PR #354でmain統合/恒久収載/作業木整理済み。greedy約35%悪化と旧長literal失敗も含む記録はgate-proofs 5-df/5-dg。#355の行span投影と#356の固定frame5条件もPR #357でmain反映/収載/整理済み（gate-proofs 5-dh）。場所照合#358はPR #359でmain統合/恒久収載/整理済み（5-di）。描画端点#360はPR #361でmain統合/恒久収載/整理済み（5-dj）。着色glyph経路#362はPR #363でmain反映/収載/整理済み（5-dk）。画面外clip#364は固定条件未達で不採用、製品を戻しPR #366で記録統合/恒久収載/整理まで完了（5-dl）。外部IME overlay差#365は未解決の別Issue。
> Issue ごとの経緯は[日報](../reports/)、コマンドと数字は [gate-proofs](../quality/gate-proofs.md)。ここには書かない（Issue #124）。
> 再開地点は[2026-10-10の引き継ぎ](../handoffs/2026-10-10.md)。高速化の第一陣は#334 / PR #336へ統合済み。第二陣#337〜#340は#341で統合受理し、正式8本・追加25場面と保存3通りを確認した。結果は[gate-proofs 5-db/5-dc](../quality/gate-proofs.md)。
> hideの「閉じた処理の前後比較で規約内の高速化を試す」指示に沿って26候補を整理した。[採否の正本](../design/2026-10-10-speed-candidate-disposition.md)は20件を全部/一部採用、2件を実験不採用、4件を未実験で見送り。R3のtintは#362、C8の確定検索は#376で追加比較して採用。R8、IO6〜IO8の未実験を明記し、全26件を速度実験済みとはしない。splitも保留。

## 運用（2026-10-03）

- 既定は現在のサナの単体実行。hide が現在の作業で分担を明示した範囲では、その指定を優先する（AGENTS.md）。2026-10-03 の #272 の仕上げは、hide の指示で設計サナが設計・判断・受理、SOL の実装サナが調査・計測器・CI を担当した。この分担を次の作業の既定にはしない。
- 繰り返す手順は `eng/` のスクリプト、道具の出力は `out/`。依頼書の型は [`docs/templates/implementation-seat-brief.md`](../templates/implementation-seat-brief.md)。過去の ADR 0038 / 0039 にある固定モデルや一律の分担より、現在の AGENTS.md と hide の指示を優先する。

## いまの Issue と次の順

2026-10-08: #304の設計・実装・対象検証を受理。今回のhide指定でAstraの設計サナが判断と受理、SOLの実装サナが実装・検証器・独立レビュー、LUNAが文書と整理の調査を担当した。次の作業は既定の単体実行へ戻し、現在のhide指示があればその範囲で優先する。[最新引き継ぎ](../handoffs/2026-10-08.md)。

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
| 9 | #291 長い日本語行の単画面の入力遅延（ADR 0069〜0075、0072は実験不採用） | **済み**。文字組み・ステータス・字体選択・可視字形・幅索引。長行の 1 打鍵 約 326 → 約 7 ms。独立レビュー・速さのゲート 7 本・試用で受理（D41）。[PR #293](https://github.com/hideyukiMORI/nene-nib/pull/293)、[gate-proofs 5-cq](../quality/gate-proofs.md#5-cq--改善一式の統合の受理issue-291adr-0075施主決定-d41) |
| 10 | #298 長い行の 1 打鍵を速さのゲートに足す（ADR 0075 決定 7） | **済み**。8 本目 `key-to-frame-single-long-line`・実機の基準値 5.431 ms（床 2 ms が効く）。CI の指紋は記録だけ。[PR #301](https://github.com/hideyukiMORI/nene-nib/pull/301)、[gate-proofs 5-cs](../quality/gate-proofs.md#5-cs--長い行の-1-打鍵のベンチと実機の基準値issue-298adr-0075) |
| 11 | #299 SDK の固定署名の恒久規則（ADR 0076・CPP-019・CNF-012） | **済み**。SDK が固定した COM の署名は印 `// SDK-ABI:` と表 `eng/sdk-abi-signatures.json` で受け、本体 6 行以内を機械が見る。WVR-0001 / 0002 は閉じて waiver は none。[PR #302](https://github.com/hideyukiMORI/nene-nib/pull/302)、[gate-proofs 5-cr](../quality/gate-proofs.md) |
| 12 | #300 字体選択と字形の保持の契約を CTest へ・取り付けの失敗で窓を終わらせない（ADR 0077） | **済み**。CTest `nib_window`（1521 checks・窓を作らない）。試験で「装飾を断っても字形の保持が残る」破れが見つかり、棄却の印で直した。22 場面 0 画素差・速さ 8 本は基準内。[PR #305](https://github.com/hideyukiMORI/nene-nib/pull/305)、[gate-proofs 5-ct](../quality/gate-proofs.md) |
| 13 | #303 F1 で操作の一覧を開き、その場で実行する（D43・ADR 0078） | **済み**。操作の名前・鍵・実行を core の 1 つの表（`operation_bindings` `operation_texts`）に寄せ、鍵と一覧が ui の `run_operation` の 1 本で実行する。F1 と Ctrl+P の面の `?`。対象の試験・速さ 8 本・実機 31 場面を受理。[gate-proofs 5-cu](../quality/gate-proofs.md) |
| 14 | #304 操作の案内を空の文書とステータスバーに出し、設定 `guide` で消せるようにする（D42） | **実装・検証済み**。操作表から本文/ステータスを排他的に表示し、設定は旧v1を残してv2へ保存。ADR 0079・[PR #313](https://github.com/hideyukiMORI/nene-nib/pull/313)・[gate-proofs 5-cv](../quality/gate-proofs.md) |
| 15 | #309 Ctrl+P の面の「候補なし」の文字を行の名前の左端にそろえる | **済み**。0 件の文字を `palette_row_label` の欄に書く（ADR 0060 決定 9）。候補 1 件の面は 0 画素差、0 件の面は 3 場面で左端がそろう。[PR #317](https://github.com/hideyukiMORI/nene-nib/pull/317)、[gate-proofs 5-cx](../quality/gate-proofs.md#5-cx--ctrlp-の面の候補なしを行の名前の欄に書くissue-309adr-0060-決定-9) |
| 16 | #312 本文の文字拡大でF1の長いキー表示が切れる | **済み**。鍵の書式に本文の倍率を掛けず 12 DIP（ADR 0078 決定 12 に 1 文）。13.5 pt は 0 画素差・24 pt で `Ctrl+Shift+S` が欄に収まる。[PR #316](https://github.com/hideyukiMORI/nene-nib/pull/316)、[gate-proofs 5-cw](../quality/gate-proofs.md#5-cw--f1-の鍵の文字を本文の倍率で拡大しないissue-312adr-0078-決定-12) |
| 17 | 高速化 #320〜#324・#330・#335 | **実装・対象検証・局所比較済み**。#334 / PR #336で統合受理。履歴/レジスタの保持サイズ由来の入力の崖を除去し、鍵列も共有。全比較値と採否はgate-proofs 5-cz、正式8本・35場面は5-da。全候補を実験済みとはしない |
| 18 | #333 ThinLTO | **実験不採用**。実bitcodeの現シンボル検査に未対応で、主要局所区間の明瞭な利益もなし。ゲート・allowlist・製品flagsを変更しない |
| 19 | #331 既存版の混雑時open正式ゲート失敗 | 既存の失敗記録を保持。新候補a64a5d4の正式8本成功とは分ける |
| 20 | 高速化第二陣 #337〜#341 / [PR #342](https://github.com/hideyukiMORI/nene-nib/pull/342) | **実装・対象検証・統合受理済み**。行取得と位置計数、一覧の照合/絞込、UTF16/CP932変換。固定14比較は13観測・caret1件は分解能未満。正式8本は0退行、追加25場面0画素差、実保存3通り成功。採否と限界は5-db/5-dc |
| 21 | 高速化第三陣 #346〜#351 | 保存・collect・検索列挙はPR #352で統合/整理済み。offset Aと照合器も#353 / PR #354で統合/収載/整理済み。代償と限界を5-df/5-dgに保持 |
| 22 | 行内span #355 / 固定frame比較 #356 | PR #357で統合/収載/整理済み。直接検証・固定5比較・GUI・正式4成功（5-dh） |
| 23 | 場所照合の一時連結 #358 | PR #359でmain反映/収載/整理済み。名前/場所を短縮、末尾絞込約2µs増加は残る（5-di） |
| 24 | renderer端点 #360 | PR #361でmain反映/収載/整理済み。密な検索/置換の応答を短縮（5-dj） |
| 25 | 着色glyph経路 #362 | PR #363でmain反映/収載/整理済み。混在長行を短縮、ASCII横ばい/短行4.5µs増（5-dk） |
| 26 | 画面外clip #364 | 旧実験は不採用、PR #366で記録統合/収載/整理済み（5-dl）。独立再評価#372は撮影不成立と入力外乱で保留（5-dm）。製品採用なし |
| 27 | 外部IME候補overlayの画素差 #365 | 同旧exeでも同じ6px差を確認。候補窓列挙の見落とし・完全画素取得・OS内部原因が未解決 |
| 28 | 多行frameの予約 #367 | 設計・製品一挿入・対象検証・固定比較・7画面0px・正式8指標成功。独立レビューで技術受理、通常PR/CI/恒久収載/監査整理へ（5-dn） |
| 29 | 一時停止の日報・引き継ぎ #368 | PR #369で保存・main統合・収載・文書用作業木整理済み |
| 30 | 再実験と継続改善の再開指示 #370 | 新しい根拠、固定再実験、採用/不採用/保留、その後の#367と継続改善の順を指示書へ保存 |
| 31 | session入力変化の出所 #373 | #372の固定系列を無効停止。出所は不明、再試行せず記録 |
| 32 | その他: 一般Ex / 複雑な書記素境界 | 未起票 |

2026-10-02 に統合（11 回目の区切り）: #264 Ctrl+P の面の日本語入力（ADR 0061・施主決定 D31・D32・PR #269）・#270 面は絞り込みの結果を持ち frame には見えている行だけ（ADR 0062・施主決定 D33・D34 も仕様へ・PR #273）・#271 裏のワーカー 1 本とフォルダの列挙（ADR 0062・PR #274）。

2026-09-30 に統合（8 回目の区切り）: #258 Ctrl+P を開くと開いているタブの一覧が出て行頭の記号で出どころを絞る・#259 閉じたファイルを履歴に覚え Ctrl+P の一覧から開く（どちらも ADR 0060・施主決定 D28〜D30）。Ctrl+P の統合（FR-006）は 5 本のうち 2 本が main に入った。

2026-09-30 に統合（7 回目の区切り）: #252 窓を閉じるときに開いていたタブを `session.v1` に覚える・#253 ファイルを指定せずに起動したとき前回のタブを戻し見るときに読む（どちらも ADR 0059・施主決定 D24〜D27）。前回のタブの復元（FR-009）は main に入った。

2026-09-30 に統合（6 回目の区切りまで）: #238 タブの帯とマウス（ADR 0056 の決定 8・9・12）・#239 タブの鍵（決定 10）・#240 タブの一覧と Ex の `tabnext` と Vim の `gt` `gT`（ADR 0057）・#248 Ctrl+Tab は最近使った順（ADR 0058・施主決定 D23）。#238 と #239 の merge は 09-29 の夜。複数タブの 4 本の縦切りは全部 main に入った。

2026-09-29 に統合: #204 数字レジスタ `"0`〜`"9` と小削除 `"-`（ADR 0050）・#210 クリップボードのレジスタ `"+` `"*`（ADR 0051）・#209 `.` の記録は待ちの状態の数字を残す・#205 改行を含む文字単位の `p` `P` のキャレット・#206 NORMAL の `X`・#208 `u` と Ctrl-r の後のキャレット（ADR 0052）・#216 Vim の 1 文字と結合文字（ADR 0053）・#222 `W` `E` `B` `ge` `gE`・#224 後ろ向きの語の移動が本文の先頭に当たる形・#226 前向きの語の移動の失敗の印・#229 fixture の鍵の記法の表の 1 本化（ADR 0054）・#235 貼り付けた本文の改行を文書の形に揃える（ADR 0055・D19）・#230 fixture の再生は `:normal!` と同じ打ち切り・#237 複数タブの状態（ADR 0056・D20〜D22）。候補（未起票）: 巨大な削除の後の打鍵のベンチとレジスタの本文の共有・oracle に undo の塊を区切る記法。

2026-09-23 に統合: #124・#131・#141・#130・#144・#146（usage の集計）・#117（`^M` の描画・ADR 0040）・#151・#147（C1 の 4 桁）・#152・#148（incsearch・ADR 0041・既定オンは施主決定 D18）・#162・#160（単体テストの分割・ADR 0042）・#165・#140（`assert_uncovered`・`measure`）・#168（Ctrl-G / Ctrl-T・ADR 0043）・#172（D18）・#175 Tab の tab stop（ADR 0045）・#174 add の chunk 化（ADR 0044）・#176 マクロ `q` `@`（ADR 0046）・#180 `recording @a`・#179 16 MiB の打鍵ベンチ・#184 改行の索引の共有（ADR 0047）・#190 oracle の `q` の拒否を狭める・#191 `erase().insert()` を `replaced` に・#193 名前つきレジスタ `"a`（ADR 0048）・#198 `VimKeyTable` の切り出し・#200 `<Space>` `<BS>`（ADR 0049）。

splitの下準備はD40でhide了承済み。本実装は試作結果から保留。その他の候補の順は未確認。open の一覧は `gh issue list --state open` が正。

## いまの数字（2026-10-10・差分ごとの検証済み実装）

| 項目 | 値 | 正本 |
| --- | --- | --- |
| Vim fixture | 1853 件（`undo-caret-*` 109 件・`combining-*` 92 件を含む・`macro-*` 20 件は `register` 欄で再生だけ・`register-*` は数字と小削除の 82 件を含む・`space-*`。`"+` `"*` は fixture にできず契約） | `tests/vim/VimFixtures.hpp` の 5 行目（CNF-010） |
| 既定の `nib_tests` | selectors45。既存選択契約を直接呼ぶframe-selectionを追加。全件checksは未測。直接境界の個別成功と使い分ける | [gate-proofs 5-cz〜5-de](../quality/gate-proofs.md)・NibTests.cppが正本 |
| ADR | 0101まで（0100は#367技術受理、0101は実験保留、0072/0087/0099は実験不採用）。splitの製品採用は保留 | [`docs/adr/README.md`](../adr/README.md) |
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
