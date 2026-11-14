# 1.0.0 の公開（2026-11-14）

公開の記録（雛形 [release-1.0.0-draft.md](release-1.0.0-draft.md) から写した・#1657）。この文書 PR で埋めたのは前半（公開元・根拠 (a)〜(e)・候補の扱い・既知の限界）だけで、「梱包」「GitHub の公開」「Microsoft Store の提出」「公開の結果」は本番の梱包の後の文書 PR で埋める。**この記録を置いた時点では、1.0.0 はまだ公開も申請もしていない**（公開版は `v0.1.0-beta.13`）。
1.0.0 は Microsoft Store に出す最初の版（施主決定 D88）。GitHub は同じ exe の ZIP を tag `v1.0.0` の pre-release（題 `NeNe Nib 1.0.0 Beta`）で出す。手順の正本は [DEVELOPMENT_WORKFLOW.md](../DEVELOPMENT_WORKFLOW.md) の公開の節、Store の欄は [submission.md](../store/submission.md) と [msix.md](../store/msix.md)。

**範囲**: 製品のソースは beta.13（`3d3859e6`）に、表示の直し 1 本（#1658）を足した物。beta.13 の後の commit は 6 本（#1648 `411b8cb3` 文書・#1620 `9944470c` 版 1.0.0 と梱包の道具・#1650 `1abf1869` 文書・#1655 `238d56b7` 文書・#1660 `3ba9a991` 文書・#1658 `a4c0d40b` 製品）。`git diff 3d3859e6 238d56b7 -- src third_party` は空で、#1620 で製品のビルドの入力が変わったのは `CMakeLists.txt` の版の 1 行（`project(NeNeNib VERSION 0.1.0 …)` → `1.0.0`）だけ。

**この記録を最初に置いた後（起点 `238d56b7`）に main に入った物**（2026-11-14）:

- **製品の変更 1 本**: #1658（PR #1665・main `a4c0d40badc829cdaeb406f308622aa4231f7a92`・2026-11-14T07:38:28Z に squash merge）。F1 の一覧の鍵の札が `Ctrl+Shift+Spac` で切れる（3 言語）のと、最小の窓のステータスバーでモードの文と行と桁が重なる、の表示の直し。見つけたのは今日の撮影（3 言語の目視・下の「今日の調査」）。鍵の欄は札の実測の幅まで広げ、入らなければ欄ごと出さない。ステータスバーはモードの文の右から 6 DIP（96 DPI で 6 px・120 DPI で 8 px）空かない項目を、行と桁 → 表示形式の印 → 案内の順に落とす。収まるときの配置は変わらず、当たり（`status_bar_hit`）も変えない（ADR 0078 と ADR 0154 の追補 2026-11-14）。出荷を止める種類（落ちる・失う）の不具合ではないが、看板の鍵の名前が切れているのは最初の版の見た目として直す、と設計席が決めて公開の前に入れた。根拠は下の (a)〜(d)。
- **文書の変更 2 本**: #1655（PR #1656・main `238d56b7`・掲載文・PRIVACY・README・msix の手順の食い違いの直し）と #1660（PR #1661・main `3ba9a991`・Store の撮影用の見本 `docs/store/samples/README.md` から表を外した。Markdown プレビューは表を描かない（md4c の表の flag を立てていない仕様）のに見本に表があり、撮影の 2 枚目に生の `|` が出ていたため）。

## 公開元

| 項目 | 値 |
| --- | --- |
| 公開元の commit（main） | この記録を置いた文書 PR（#1657）の merge の commit（40 桁は merge の後に `git rev-parse origin/main` から写し、公開の結果の節に書く） |
| 前の公開 | `v0.1.0-beta.13`（`3d3859e69fc8023e1374ff4adae5af34ae916c98`・tag object `04b55b0e98fc4e04d3cf5a35a56de862b110a32a`・2026-11-14 00:16・[記録](beta13-release-2026-11-13.md)） |
| 版（`project()`） | `1.0.0`（#1620 / PR #1621・main `9944470c`・2026-11-14 01:46 JST） |
| 梱包の作業木 | 公開元の detached 作業木（HEAD = origin/main = 公開元・未追跡 0）。候補の木 `9944470c` は使わない |

## 候補 `v1.0.0-candidate-9944470c` は予行（上げる物ではない）

`D:/NeNeNib/releases/v1.0.0-candidate-9944470c/` は main `9944470c` から作った ZIP と `-Store` の MSIX で、手順の 1a（この記録の merge の commit から作る）を通っていない（`zip/package.json` の `commit` は `9944470c`・`verification.path` は雛形 `docs/quality/release-1.0.0-draft.md`）。Release の exe は作るたびにバイト列が変わる（PE の時刻が入る）ので、同じソースから作り直しても同じ exe にはならない: 予行（作業枝 `04c2e3ce`・`D:/NeNeNib/evidence/20261114-release-rehearsal/README.md`）の exe は `4dae2ccee1344760b945607e5d17c235848e2bfa7c3c55be6341a96c11b661b1`、候補（`9944470c`）の exe は `12356835469135a5a4937ff3188700f02294671839a43d5b890f937df5a1b1cb`（どちらも 3,623,424 bytes・`git diff 04c2e3ce 9944470c -- src third_party CMakeLists.txt eng tests` は空）。候補を試して申請したあとで GitHub の ZIP を公開元から作り直すと、「GitHub と Store は同じ commit の同じ exe」が崩れ、直すには版を上げるしかない（同じ版で作り直さない）。だから**本番の ZIP と MSIX は公開元で 1 回だけ作り、施主はその本番の物を試して申請する**（試した物 = 上げる物・設計席の決め 2026-11-14・走り書き `D:/NeNeNib/outputs/20261114/notes.md`）。候補は手順と道具の予行としてだけ扱う。施主の試しで直しが要れば、未公開なので同じ版のまま公開元を作り直す。なお候補を作った後に main へ #1658（`a4c0d40b`）が入ったので、候補 `9944470c` の exe は公開する物とバイト列だけでなく製品のコードも違う（`git diff 9944470c a4c0d40b -- src` は 9 ファイル）。

## 根拠（再利用する検証）

製品のソースは beta.13 に #1658 を足した物。#1658 は (a)〜(d) の新しい根拠（ゲート・独立レビュー・CI・席の検証）で受け、それ以外の製品の検証は beta.13 の根拠を再利用する（QLT-012）。版 1.0.0 で変わった物（版の 1 行・梱包の道具・ZIP の `README.txt`・文書）は (b)・(c)・(e) で受ける。

- (a) 速さのゲート（QLT-014）: **#1658 の木 `41ecd0041ffdee00c97087178683bfcefc71d259`** の Release（作業木 `D:/NeNeNib/worktrees/1658-chord-label-and-status-overlap`・HEAD は `41ecd004` で未追跡 0・`NeNeNib.exe` 3,624,960 bytes・SHA-256 `5bbe281d0936fa0a0aaeb09c4944352d857d37a264eb5f7efc02c1a16238b60b`・Release のビルドの `FAILED|error:|warning:` は 0 行）で **8 benches checked・0 regression・0 unmeasurable**（2026-11-14 15:55〜15:56・5 runs・中央値 起動 44.500 / 窓 34.075 / 単打 0.619 / 200 打鍵 2.970 / open 16 MiB 49.808 / 16 MiB で 200 打鍵 2.604 / 面 1.093 / 長い行 0.963 ms・測る前の負荷の読み 0 / 4 / 1%・ビルドの process 0・記録 `D:/NeNeNib/outputs/20261114/gate-fix1658-41ecd004.log` と `-load.txt`・`release-and-gate-1658.log`）。**squash した main の木は、ゲートを測った木と製品の入力で同じ**: `git diff 41ecd004 a4c0d40b -- src third_party tests CMakeLists.txt eng` は空（`41ecd004` と `a4c0d40b` の差は #1660 の文書 2 ファイル `docs/store/samples/README.md` と `docs/store/screenshots.md` だけ）。この後の記録の PR は文書のみ。
  - その前の版の値（製品が beta.13 と同じだった木・記録として残す）: まとめ枝 33 の木 `0e5a2e3e` の Release で **8 benches checked・0 regression・0 unmeasurable**（2026-11-13 23:19〜23:21・中央値 起動 44.293 / 窓 32.818 / 単打 0.541 / 200 打鍵 2.843 / open 16 MiB 52.389 / 16 MiB で 200 打鍵 2.758 / 面 1.084 / 長い行 0.987 ms・負荷 4〜7%・記録 `D:/NeNeNib/outputs/20261113/gate-integration33-0e5a2e3e.log` と `-load.txt`・基準値は [speed-adoption-2026-10-18.md](speed-adoption-2026-10-18.md)）。`git diff 0e5a2e3e cc9c7f44 -- src third_party tests CMakeLists.txt eng` は空。`git diff cc9c7f44 238d56b7 -- src third_party tests CMakeLists.txt eng` は 6 ファイル（`CMakeLists.txt` の版の 1 行・`eng/package-release.ps1` `.py`・`eng/release/README.txt.in`・`eng/test-package-release.py`・`tests/conformance/test_package_msix.py`）で、製品のビルドの入力は版の 1 行だけ（ほかは梱包の道具・梱包の文・試験）。どれも速さに関わらないので、梱包の exe では測らない（beta.9〜beta.13 と同じ形）。
- (b) 独立レビュー:
  - beta.13 までの製品の変更は beta.13 の記録の (b) のとおり（[beta13-release-2026-11-13.md](beta13-release-2026-11-13.md)）。
  - #1658（PR #1665）は **V-1665**（読むだけの席・2026-11-14・`D:/NeNeNib/outputs/20261114/review-1665/review.md`・対象は `70d26dd1c5ba7533009b1d5ea3c9df92ba982a72` を git の object から読んだ・実測なしで数字は式からの読み）。判定は「**70d26dd1 のままなら merge してよい**」（必須 0）で、そのとき別の席が入れていたモードの文と行と桁の間 8 → 6 DIP の変更には**必須 1**（`tests/unit/CoreTests.cpp` の `verify_status_bar_fitted` の `marked` が、6 DIP では limit = 122 + 150 + 6 = 278 で印の左端 278 が `278 < 278` を満たさず落ちる → モードの幅を 151 以上に・ADR 0154 の数字と PR 本文の書き直し）。この必須は次の commit `41ecd004`（間を 6 DIP）で片付いた: `git show 41ecd004` で、`marked` はモードの幅 152（122 + 152 + 6 = 280 が 278 を越えて印を落とす）に直り、境目ちょうどの 150 で印が残る例（`a mark that starts exactly at the gap after the mode text stays`）が足され、ADR 0154 の追補の数字も同じ commit で直っている。R0（退行の反例探し）では、普通の窓で配置は動かない（`fitted == layout`）・当たりは測らない配置のまま・保持の枠は最大 11 のまま・足した費用は保持の枠の `GetMetrics` が 1 frame に 2〜3 回で、5000 件の面（鍵の無い候補）では 0。軽い指摘 5（狭い窓で行と桁が出たり消えたりする・規則が core の 2 関数に重複 ほか）と後続 2（150% / 200% の DPI・利用者の字体・renderer の配線の試験／最小の窓と 734〜770 px の帯の実機の目視）は 1.0.0 の後。
  - #1658 の席の検証（PR #1665 の本文）: 間を 6 DIP にした HEAD `41ecd004` の exe で `nib_window_tests --language` 9,670 checks・0 failures／`nib_window_tests` 既定 15,291 checks・0 failures／`nib_tests` 既定 2,405,548 checks・`--guide` 544・`--command-palette` 886 pass／conformance 0 violation／clang-format の差分 0／Debug の全 target のビルドで警告 0。直す前の答えに一時的に戻すと `--command-palette` 6 件・`--language` の鍵 10 件（前の HEAD で測定）、6 DIP の後にステータスの 2 本だけ戻すと `--language` 1,246 件・`--guide` 2 件が落ちる。測った集合はステータス 1,320 形（3 言語 × モードの文 11 × 行と桁 5 組 × 96 / 120 DPI × 窓 4）と、面は 2 DPI × 窓 3 × 最も広い鍵。`ctest --test-dir build` 22 / 22 pass と symbols 0 violation は前の HEAD `70d26dd1` の値で、`41ecd004` は core の定数 1 つ（`status_text_gap_dips` 8 → 6）と unit の期待 1 か所と ADR の数字だけの差なので再利用した（定数を読む試験は HEAD の exe で流し直した）。
  - 版 1.0.0 と梱包の道具の PR #1621（`9944470c`）は V-1621（読むだけの席・2026-11-14・`D:/NeNeNib/outputs/20261114/review-1621/review.md`）。判定は「公開の前に直す」（必須 2・軽い指摘 5・後続 4）で、道具のコードに公開を止める穴は無い。必須の片付け: **M1**（Store の掲載文 3 言語が「`:s` はまだありません」と書いていた）は #1655 / PR #1656（main `238d56b7`）で直した。**M2**（候補は手順の 1a を通っていない・exe は作るたびにバイト列が変わる）は、本番をこの記録の merge の commit から作り、施主はその本番の物を試す、で片付ける（上の「候補は予行」）。軽い指摘の L2（自己署名の手順の `-store.msix` の名前）と L5（`submission.md` の `MinVersion`）は #1656 で済み、L1（DEVELOPMENT_WORKFLOW の ZIP の出力先）と L3（`entries` の数）はこの PR で直した。L4 と後続 F1〜F4（`-Verification` が雛形を受ける・同じ版の作り直しを機械が止めない・`--store` が ZIP の `package.json` を見ない・`-Store` を通す試験が無い）は 1.0.0 の後の Issue。
  - 出荷前の点検 REL-A（読むだけ・2026-11-14・`D:/NeNeNib/outputs/20261114/rel-a/readiness.md`）: 開いている Issue 97 本を読み、**1.0.0 を止める製品の不具合は 0 件**（落ちる・数秒以上止まる・本文を失う・黙って違う中身で保存する、のどれにも当たる物が無い）。配る物（候補の ZIP 4 個・MSIX）に個人のパスと `.pdb` は 0・import は OS の DLL だけ・`MinVersion` `10.0.22621.0`。外に出す文の食い違い 9 件は #1656 で直した。REL-A が**確かめていないこと**: WACK の結果・自己署名での導入と起動（packaged の振る舞い）・BlockMap の SHA-256 の検査・異常な入力（#519）を Release の exe でまとめて見ること。窓は出しておらず、ビルドも起動もしていない。
- (c) CI: PR #1621 は merge の道具の run [43460142800](https://github.com/hideyukiMORI/nene-nib/actions/runs/43460142800)（`workflow_dispatch`・題 `Merge PR #1621 at 04c2e3ce35ed53e847c4024abccef2654060d023`・`check` と `build-and-test` が success・`check.yml` は `expected_head` を checkout する）→ squash merge で main `9944470c`（2026-11-13T16:45:53Z）。#1658 は PR #1665 の merge の道具の run [43568791652](https://github.com/hideyukiMORI/nene-nib/actions/runs/43568791652)（`workflow_dispatch`・題 `Merge PR #1665 at 41ecd0041ffdee00c97087178683bfcefc71d259`・`check` と `build-and-test` が success・2026-11-14T06:57:20Z〜07:38:23Z）→ squash merge で main `a4c0d40b`（2026-11-14T07:38:28Z）。これが製品の最後の CI（その前は beta.13 の記録の (c)・run 43442097904 → `cc9c7f44`）。#1650 #1656 #1661 とこの記録の PR は文書のみなので、merge 直前の CI は要らない（CLAUDE.md §4）。
- (d) 施主の試用（D41）: 2026-11-13 20:4x〜21:3x の表 A〜D（表示言語・アイコン・Vim の命令・決めてほしいこと）が OK（表 `D:/NeNeNib/evidence/20261113-trial/README.md`・結果は [beta13-release-2026-11-13.md](beta13-release-2026-11-13.md) の (d)・施主決定 D89）。beta.13 までの製品はこの試用を再利用する。**#1658 の直しは施主がまだ見ていない**: 表示の小さな直しで、施主が不在の間に設計席の判断で公開の前に入れた。施主は本番の物で、packaged の確かめと一緒に見る。**packaged（MSIX）の起動と振る舞いの確かめと WACK はまだ**（施主の手・[submission.md](../store/submission.md) の「packaged の実機の確かめ」と [msix.md](../store/msix.md)・本番の `-store.msix` で行う）。
- (e) 梱包の道具の試験: V-1621 が 2026-11-14 に回した `python eng/test-package-release.py --work-dir <D 側の新しいディレクトリ>` → **10 件 OK**（`D:/NeNeNib/outputs/20261114/review-1621/test-package-release.log`）と `python -m unittest tests.conformance.test_package_msix` → **18 件 OK**（`test-package-msix.log`）。`eng/test-package-release.py` は `--work-dir` が要る手動の試験で、CI には載っていない（V-1621 の L4）。

### 今日の調査（2026-11-14・製品を変えていない席）

- 出荷前の点検 REL-A: 上の (b) のとおり（`D:/NeNeNib/outputs/20261114/rel-a/readiness.md`・止める製品の不具合 0 件）。
- 差分 fuzz FZ2（main `1abf1869` の再生器・`D:/NeNeNib/outputs/20261114/fz2/`・席の報告の本文が正で `report.md` は無い）: **9,600 件・落ち 0・止まり 0**。生成器 6（seed 201〜210・3,000 件）一致 99.97%（違い 1 件は閉じた #1483 と同じ形 `zb<C-f>`・画面移動の端で落ちではない・公開の後に確かめる）・生成器 5（3,000 件）100%・Ex の行（FZ1 の生成器・seed 11〜13・比べられる 2,097 件）90.3%・語を足した版（1,079 件）92.0%。違いは既知の種類で、**新しい種類は 1 つ**（`:s|` `:&|` の後の `|` を読まない → #1664・未着手）。
- 撮影 SHOT1（`D:/NeNeNib/evidence/store-1.0.0/`・exe は候補の `12356835…b1cb`・1600 × 1000・120 DPI）: 目視 66 枚（3 言語 × 22・`review/<lang>/`）と Store の下書き 15 枚（3 言語 × 5・`screenshots/<lang>/`）。指摘 12（中 6・軽 6）のうち製品の直しにしたのが #1658、撮影用の見本の表は #1660 で外した。**Store の 2 枚目（Markdown プレビュー）と目視の 08 は本番の exe で撮り直す**（下書きの 2 枚目には表の生の `|` が写っている）。

## 既知の限界

beta.13 の記録の「既知の限界」をそのまま引き継ぐ（製品は beta.13 ＋ #1658）。それに加えて、REL-A が 1.0.0 を止めないと決めた物（`D:/NeNeNib/outputs/20261114/rel-a/readiness.md` の A 節）:

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

今日わかった物（どれも 1.0.0 を止めない）:

- 撮影の残りの指摘: 日本語と中国語の表示言語でも、Nib 自身の Ex の断りなどは英語のまま（#1585）。英語の表示言語で Ctrl+P の面の行が等幅の字体で描かれる。中国語の表示言語で、かなを含むファイル名が中国語の UI の字体で描かれる。知らせを出している間、エクスプローラーの縁がステータスバーの左に出る。ほか軽い物は 1.0.0 の後に Issue。
- ステータスバーは、入らないとき行と桁 → 表示形式の印 → 案内の順に落とす（#1658・ADR 0154 の追補）。最小の窓（360 DIP）では行と桁がほぼいつも落ち（96 / 120 DPI とも 165 形のうち 164・残るのは日本語の `通常` ＋ `行 17, 桁 4`）、狭い窓（96 DPI でおよそ 535〜610 px・V-1665 の読み）ではモードの文と知らせが変わるたびに行と桁が出たり消えたりする。
- **設定の互換**（V-1668 の「設定の互換」の節と後続 F1・`D:/NeNeNib/outputs/20261114/review-1668/review.md`・コードの読み）: 1.0.0 の設定の読み手は、知らない鍵のある `settings.v4` を `Failure::malformed` と読む。そのセッションは既定の設定（配色・字体など）で起動し、以後の設定の保存は全部拒む。ファイルは書き換えず消しもしない（壊さない・捨てない・保存は拒む）。だから後の版が v4 に鍵を足して書くと、1.0.0 に戻した人や、新しい版と 1.0.0 を同じ置き場で併用する人がこの状態になる。設計席の決め: 1.0.0 の読み手は ADR 0114 の厳密な読みのまま止めず、**1.0.0 の後は v4 に鍵を足さない**。鍵を足す PR（#1668 の `autoindent` が最初）は、施主に聞いてから「版を上げる（v5・旧版は残す）」か「知らない鍵を読み飛ばして残す読み手」を先に入れる。`explorer_width`（#1380）も同じ形で v4 に足されていて、どの公開版がそれを読めないかは公開の後に調べる。
- 複数のファイルをドロップすると、入力行の間・IME の変換中・Vim の待ち・Ctrl+Tab の歩きの間（と、裏で読む大きいファイルを 2 本以上落としたとき）は最後の 1 本だけ開き、知らせも出ない。失うのは開くはずのタブだけで、本文と未保存の物は失わない（V-1662 の後続 F1・`D:/NeNeNib/outputs/20261114/review-1662/review.md`・公開の後に Issue）。

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
