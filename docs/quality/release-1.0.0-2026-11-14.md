# 1.0.0 の公開（2026-11-14）

公開の記録（雛形 [release-1.0.0-draft.md](release-1.0.0-draft.md) から写した・#1657）。この文書 PR で埋めたのは前半（公開元・根拠 (a)〜(e)・候補の扱い・既知の限界）だけで、「梱包」「GitHub の公開」「Microsoft Store の提出」「公開の結果」は本番の梱包の後の文書 PR で埋める。**この記録を置いた時点では、1.0.0 はまだ公開も申請もしていない**（公開版は `v0.1.0-beta.13`）。
1.0.0 は Microsoft Store に出す最初の版（施主決定 D88）。GitHub は同じ exe の ZIP を tag `v1.0.0` の pre-release（題 `NeNe Nib 1.0.0 Beta`）で出す。手順の正本は [DEVELOPMENT_WORKFLOW.md](../DEVELOPMENT_WORKFLOW.md) の公開の節、Store の欄は [submission.md](../store/submission.md) と [msix.md](../store/msix.md)。

**範囲**: 製品のソースは beta.13（`3d3859e6`）と同じ。`git diff 3d3859e6 238d56b7 -- src third_party` は空。beta.13 の後の commit は 4 本（#1648 `411b8cb3` 文書・#1620 `9944470c` 版 1.0.0 と梱包の道具・#1650 `1abf1869` 文書・#1655 `238d56b7` 文書）で、製品のビルドの入力が変わったのは #1620 の `CMakeLists.txt` の版の 1 行（`project(NeNeNib VERSION 0.1.0 …)` → `1.0.0`）だけ。

## 公開元

| 項目 | 値 |
| --- | --- |
| 公開元の commit（main） | この記録を置いた文書 PR（#1657）の merge の commit（40 桁は merge の後に `git rev-parse origin/main` から写し、公開の結果の節に書く） |
| 前の公開 | `v0.1.0-beta.13`（`3d3859e69fc8023e1374ff4adae5af34ae916c98`・tag object `04b55b0e98fc4e04d3cf5a35a56de862b110a32a`・2026-11-14 00:16・[記録](beta13-release-2026-11-13.md)） |
| 版（`project()`） | `1.0.0`（#1620 / PR #1621・main `9944470c`・2026-11-14 01:46 JST） |
| 梱包の作業木 | 公開元の detached 作業木（HEAD = origin/main = 公開元・未追跡 0）。候補の木 `9944470c` は使わない |

## 候補 `v1.0.0-candidate-9944470c` は予行（上げる物ではない）

`D:/NeNeNib/releases/v1.0.0-candidate-9944470c/` は main `9944470c` から作った ZIP と `-Store` の MSIX で、手順の 1a（この記録の merge の commit から作る）を通っていない（`zip/package.json` の `commit` は `9944470c`・`verification.path` は雛形 `docs/quality/release-1.0.0-draft.md`）。Release の exe は作るたびにバイト列が変わる（PE の時刻が入る）ので、同じソースから作り直しても同じ exe にはならない: 予行（作業枝 `04c2e3ce`・`D:/NeNeNib/evidence/20261114-release-rehearsal/README.md`）の exe は `4dae2ccee1344760b945607e5d17c235848e2bfa7c3c55be6341a96c11b661b1`、候補（`9944470c`）の exe は `12356835469135a5a4937ff3188700f02294671839a43d5b890f937df5a1b1cb`（どちらも 3,623,424 bytes・`git diff 04c2e3ce 9944470c -- src third_party CMakeLists.txt eng tests` は空）。候補を試して申請したあとで GitHub の ZIP を公開元から作り直すと、「GitHub と Store は同じ commit の同じ exe」が崩れ、直すには版を上げるしかない（同じ版で作り直さない）。だから**本番の ZIP と MSIX は公開元で 1 回だけ作り、施主はその本番の物を試して申請する**（試した物 = 上げる物・設計席の決め 2026-11-14・走り書き `D:/NeNeNib/outputs/20261114/notes.md`）。候補は手順と道具の予行としてだけ扱う。施主の試しで直しが要れば、未公開なので同じ版のまま公開元を作り直す。

## 根拠（再利用する検証）

製品のソースが beta.13 と同じなので、製品の検証は beta.13 の根拠を再利用する（QLT-012）。版 1.0.0 で変わった物（版の 1 行・梱包の道具・ZIP の `README.txt`・文書）は (b)・(c)・(e) で受ける。

- (a) 速さのゲート（QLT-014）: まとめ枝 33 の木 `0e5a2e3e` の Release で **8 benches checked・0 regression・0 unmeasurable**（2026-11-13 23:19〜23:21・中央値 起動 44.293 / 窓 32.818 / 単打 0.541 / 200 打鍵 2.843 / open 16 MiB 52.389 / 16 MiB で 200 打鍵 2.758 / 面 1.084 / 長い行 0.987 ms・負荷 4〜7%・記録 `D:/NeNeNib/outputs/20261113/gate-integration33-0e5a2e3e.log` と `-load.txt`・基準値は [speed-adoption-2026-10-18.md](speed-adoption-2026-10-18.md)）。`git diff 0e5a2e3e cc9c7f44 -- src third_party tests CMakeLists.txt eng` は空。`git diff cc9c7f44 238d56b7 -- src third_party tests CMakeLists.txt eng` は 6 ファイル（`CMakeLists.txt` の版の 1 行・`eng/package-release.ps1` `.py`・`eng/release/README.txt.in`・`eng/test-package-release.py`・`tests/conformance/test_package_msix.py`）で、製品のビルドの入力は版の 1 行だけ（ほかは梱包の道具・梱包の文・試験）。どれも速さに関わらないので、梱包の exe では測らない（beta.9〜beta.13 と同じ形）。
- (b) 独立レビュー:
  - 製品の変更は beta.13 の記録の (b) のとおり（[beta13-release-2026-11-13.md](beta13-release-2026-11-13.md)）。
  - 版 1.0.0 と梱包の道具の PR #1621（`9944470c`）は V-1621（読むだけの席・2026-11-14・`D:/NeNeNib/outputs/20261114/review-1621/review.md`）。判定は「公開の前に直す」（必須 2・軽い指摘 5・後続 4）で、道具のコードに公開を止める穴は無い。必須の片付け: **M1**（Store の掲載文 3 言語が「`:s` はまだありません」と書いていた）は #1655 / PR #1656（main `238d56b7`）で直した。**M2**（候補は手順の 1a を通っていない・exe は作るたびにバイト列が変わる）は、本番をこの記録の merge の commit から作り、施主はその本番の物を試す、で片付ける（上の「候補は予行」）。軽い指摘の L2（自己署名の手順の `-store.msix` の名前）と L5（`submission.md` の `MinVersion`）は #1656 で済み、L1（DEVELOPMENT_WORKFLOW の ZIP の出力先）と L3（`entries` の数）はこの PR で直した。L4 と後続 F1〜F4（`-Verification` が雛形を受ける・同じ版の作り直しを機械が止めない・`--store` が ZIP の `package.json` を見ない・`-Store` を通す試験が無い）は 1.0.0 の後の Issue。
  - 出荷前の点検 REL-A（読むだけ・2026-11-14・`D:/NeNeNib/outputs/20261114/rel-a/readiness.md`）: 開いている Issue 97 本を読み、**1.0.0 を止める製品の不具合は 0 件**（落ちる・数秒以上止まる・本文を失う・黙って違う中身で保存する、のどれにも当たる物が無い）。配る物（候補の ZIP 4 個・MSIX）に個人のパスと `.pdb` は 0・import は OS の DLL だけ・`MinVersion` `10.0.22621.0`。外に出す文の食い違い 9 件は #1656 で直した。REL-A が**確かめていないこと**: WACK の結果・自己署名での導入と起動（packaged の振る舞い）・BlockMap の SHA-256 の検査・異常な入力（#519）を Release の exe でまとめて見ること。窓は出しておらず、ビルドも起動もしていない。
- (c) CI: PR #1621 は merge の道具の run [43460142800](https://github.com/hideyukiMORI/nene-nib/actions/runs/43460142800)（`workflow_dispatch`・題 `Merge PR #1621 at 04c2e3ce35ed53e847c4024abccef2654060d023`・`check` と `build-and-test` が success・`check.yml` は `expected_head` を checkout する）→ squash merge で main `9944470c`（2026-11-13T16:45:53Z）。製品の最後の CI は beta.13 の記録の (c)（run 43442097904 → `cc9c7f44`）。その後の #1650 #1656 とこの記録の PR は文書のみなので、merge 直前の CI は要らない（CLAUDE.md §4）。
- (d) 施主の試用（D41）: 2026-11-13 20:4x〜21:3x の表 A〜D（表示言語・アイコン・Vim の命令・決めてほしいこと）が OK（表 `D:/NeNeNib/evidence/20261113-trial/README.md`・結果は [beta13-release-2026-11-13.md](beta13-release-2026-11-13.md) の (d)・施主決定 D89）。製品のソースは同じなので、この試用を再利用する。**packaged（MSIX）の起動と振る舞いの確かめと WACK はまだ**（施主の手・[submission.md](../store/submission.md) の「packaged の実機の確かめ」と [msix.md](../store/msix.md)・本番の `-store.msix` で行う）。
- (e) 梱包の道具の試験: V-1621 が 2026-11-14 に回した `python eng/test-package-release.py --work-dir <D 側の新しいディレクトリ>` → **10 件 OK**（`D:/NeNeNib/outputs/20261114/review-1621/test-package-release.log`）と `python -m unittest tests.conformance.test_package_msix` → **18 件 OK**（`test-package-msix.log`）。`eng/test-package-release.py` は `--work-dir` が要る手動の試験で、CI には載っていない（V-1621 の L4）。

## 既知の限界

beta.13 の記録の「既知の限界」をそのまま引き継ぐ（製品が同じ）。それに加えて、REL-A が 1.0.0 を止めないと決めた物（`D:/NeNeNib/outputs/20261114/rel-a/readiness.md` の A 節）:

- #1347: 2 つの Nib が閉じる時と起動する時に重なると、片方の `session.v2` を上書きしうる。`history.v1` も lock の無い「読む → 足す → 書く」で、片方の追記を失いうる。失うのはタブの一覧と履歴で、文書の本文ではない。
- #1583 F1: 入力行（`:`）が開いたまま、空の無題の文書にファイルを開くと、入力行が残ったまま本文が入れ替わる（失う本文は無い）。
- #1430 (1): Markdown プレビューの画像が OneDrive の「オンラインのみ」・割り当てたネットワークドライブ・UNC の先にあると、worker が数十秒止まりうる（UI のスレッドは止まらない・自動退避と開く道の読みとフォルダの列挙が待たされる）。1.0.1 の候補。
- #1631: 最後の砦は C++ の例外だけを受け、SEH（アクセス違反・stack の溢れ）では退避しない（30 秒ごとの自動退避は残る）。
- 作った入力や巨大な回数で重くなる物: 16 MiB で空白の無い 1 行の `iw` `aw`（#1441・0.4〜0.9 s の読み）・行が伸びるマクロの巨大な回数（#1451・再生は 100,000 鍵で止まる D75）・巨大な回数の `N` `gN`（#1632 F3）・病的な検索の形の `hlsearch` と incsearch（#1588 F3・#1619 L6・仕事の予算は 1.0.1 以降）。
- #739（260 文字を超えるパスのファイルダイアログの道）と #424（D ドライブの隔離 profile で設定の保存の後に応答しなくなった 1 回・原因未確定）と #519（異常な入力の Release での調査）は確かめていない。
- Shift_JIS の二重の符号: CP932 で同じ字に 2 つの符号がある所（NEC 選定 IBM 拡張 `0xED40-0xEEFC`・NEC 特殊文字 `0x8790-` の `≒ ≡ ∵` など）は、開いて保存すると触っていない行でも Windows が選ぶ方の符号に書き換わる（見た目の字は同じ・メモ帳と同じ振る舞いの読み・実測なし）。
- 壊れた UTF-8: BOM の無い UTF-8 に壊れたバイトがあると、CP932 として読めれば Shift_JIS で開く（文字化けで見え、ステータスバーに Shift_JIS と出る・保存すると CP932 で書き戻す）。

1.0.0（Store 版）で増えた物:

- F1 の「Windows の右クリックに登録する」は、packaged（Store 版）ではエクスプローラーから見えない見込み（読み・未実測。HKCU への書き込みがパッケージ専用の場所へ写る見込みで、製品は packaged かを見ていない）。PRIVACY と README は ZIP 版の動きを書き、Store 版は効かない見込みで未実測と書いた（#1656）。施主の packaged の確かめで見る。
- 中国語（簡体字）の画面の文は機械の訳で、母語の人のレビューは無い（`docs/i18n/REVIEW.md`）。
- WACK と packaged の起動はまだ（根拠 (d)）。AppxManifest の DisplayName と Description は英語の 1 つだけ（言語ごとの出し分けなし）・VERSIONINFO の CompanyName は空（REL-A の B・どちらも止めない）。

## 梱包

（本番の梱包の後に埋める。道具の出力 `package.json`・`msix.json`・`SHA256SUMS` から写す）

| 物 | 名前 | bytes | SHA-256 |
| --- | --- | --- | --- |
| exe（`build/release-<short>/NeNeNib.exe`） | `NeNeNib.exe` | | |
| ZIP | `NeNeNib-1.0.0-windows-x64.zip` | | |
| ZIP の `SHA256SUMS` | | | |
| MSIX（Store 用・署名なし） | `NeNeNib-1.0.0.0-windows-x64-store.msix` | | |
| MSIX の `SHA256SUMS` | | | |

照合:

- [ ] `package.json` と `msix.json` の `commit` が公開元・`package.json` の `verification.path` がこの記録
- [ ] `package.json` の `productVersion` と `version` が `1.0.0`、`metadata` の `fileVersion` `1.0.0.0` と `productVersion` `1.0.0`
- [ ] `msix.json` の `executable.sha256` が ZIP の中の exe と同じ（`zip.executableMatches`）
- [ ] `msix.json` の `packageVersion` が `1.0.0.0`・`store` が true
- [ ] manifest の `Resources` が `en-US` `ja-JP` `zh-Hans` の順
- [ ] `msix.json` の `entries` が 32 個（中身 30 個 ＝ exe・`LICENSE`・`THIRD_PARTY_NOTICES.txt`・`AppxManifest.xml`・`resources.pri`・`Assets` 25 枚、に makeappx が足す `AppxBlockMap.xml` と `[Content_Types].xml`）
- [ ] 展開した exe の短い起動（隔離 profile）

## GitHub の公開

（公開の後に埋める）

## Microsoft Store の提出

（本番の `-store.msix` で施主が行った後に埋める。表の形は雛形のとおり）

## 公開の結果

（公開の後に埋める）
