# 5-cl — ステータスの固定枠と単打分布（Issue #291・ADR 0070）

出典: docs/quality/gate-proofs.md の節 5-cl（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-05夜、hideから対象を局所的に閉じて速度と安定性を改善する指示と、食事中に改善・計測を進める許可を受け、現在のサナが単体で実装・検証した。前面操作は21:34 JSTに終了。通常版は`a1fd1e1`を対照O、ステータス固定枠を加えた`0dfad49d9f79e2efd7f165a35e9221f999c4a441`を候補Aと呼ぶ。Oにも本文layout再利用と重複無効化削減が入っており、元の`591b99f`との比較ではない。

### 対象と実装

本文後の区間を分けた計装版`5476c66`で、ステータス描画は単打中央値0.282ms・最大0.321ms、前フレームの本文layout解放は0.001ms・最大0.002msだった。対象をステータスに絞り、`Direct2DRenderer.cpp/.hpp`と新しい`StatusTextLayout.hpp`で最大7枠を所有する。`draw_status_bar`で枠位置を戻し、文字列全文・書式・幅・高さが同じ枠のUTF変換と文字組み生成を省く。色と原点は毎回使う。UI書式再作成前に全枠を破棄し、作成失敗では古い表示を出さない。コマンド入力の専用layout、本文cache、解放位置、入力経路は変更しない。

通常候補Releaseは1311744 bytes、SHA-256 `A68628E8E48D9D83063B26ECA1F8FE00E42797A74DB7C7369389EAE6A51918AE`。既存の作業枝`fix/291-long-line-rendering`を検証済みcommitへfast-forwardした。計装は診断枝だけに保持する。main・remote PRへの反映は行っていない。

### 表示・構造の検証

| 実行したコマンドと対象 | 確認する退行・結果 |
| --- | --- |
| 各D worktreeで`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` | 型・所有・警告・clang-tidy・リンク。診断前`5476c66`、通常`0dfad49`、診断後`9c617c0`の3版が終了0。通常版total 130.202s、診断後129.643s。記録は証拠束の`logs/`と`releases/` |
| `clang-format --dry-run --Werror src/ui/win32/Direct2DRenderer.hpp src/ui/win32/Direct2DRenderer.cpp src/ui/win32/StatusTextLayout.hpp`、`git diff --check` | 変更したC++の整形。終了0 |
| `python -B -`で既存conformanceの`waiver_checks`・`document_checks`と変更した3ソースの`source_checks`を呼ぶ限定検査 | 新しい型・状態とADRが規約に従うこと。0 findings、終了0。`291-status-layout-conformance-20261005.log` |
| `python -B D:/NeNeNib/scripts/291-status-layout-visual-20261005.py --exe <a1fd1e1のexe> --name A`、`--exe <0dfad49のexe> --name B`、`--compare` | キャッシュの誤一致、変更されたラベルや色の描画漏れ、枠上限。22場面ずつ正常終了、本文/ステータスの差分0画素。ここだけA/Bは旧版/新版。7枠を使うNORMAL/INSERT録画、停止、通知、Ex入力、通常/Vim、桁上がり、改行、幅変更、テーマ、フォント、paletteを含む。重要な状態間で画素が変化したことも確認。代表画像4枚を目視 |
| `python -B eng/protected-diff.py --base e40742a --head 0dfad49` | 保護対象を変更していないこと。終了0、fixture 1853→1853、metadata/deleted/changed/added=0、protected files changed none。scopeは33・件数未測33。製品全件テストを追加せず、件数不変とは主張しない |

自己レビューでは、`write_status`がステータスからだけ呼ばれること、通常6枠・録画最大7枠・通知4枠・Ex入力時3枠で上限を越えないこと、書式変更前の破棄、layoutを外部へ貸さず変更しないこと、色と原点をキャッシュ条件に入れないことを確認した。公開API、保存形式、基準値、waiverは変更していない。

### 計装なしの固定6組12試行

`python -B D:/NeNeNib/scripts/291-status-layout-normal-20261005.py --prepare` / `--run` / `--audit`。準備・解析器の正例/負例・実測は終了0、監査は非0（Pythonの判定hold、外側PowerShellの終了値1）。既存`keys_trial`と監査を再利用し、奇数O/A・偶数A/O、新規profile、1280×800 / DPI120 / 既定Cascadia Code 13.5ptで固定。全12本で202入力、暖機/単打の独立、提示、raw再計算、入力前/終了前の幾何、設定不在、hashを確認。欠測0・追加試行0。

| 指標 | O 中央値 / 最大 ms | A 中央値 / 最大 ms | 対応組でAが短縮 |
| --- | ---: | ---: | ---: |
| 単打 | 1.084 / 1.107 | 0.732 / 0.758 | 6/6 |
| 200文字 | 3.237 / 3.773 | 3.169 / 3.577 | 4/6 |

単打中央値は32.5%短縮。O単打の前後半差0.027ms、A単打0.008ms、A burst 0.441msは既存の安定幅内だった。一方O burstは前半3.552→後半2.922ms、差0.630msが許容0.500msを越えた。**既存総合判定はhold**であり、単打改善を総合合格と言い換えない。通常入力の恒常的な最悪時間やOS原因はこの実験では確定しない。

### 各版240回の独立単打

通常の固定試行とは別に、1起動あたり暖機1文字+40文字、名目100ms間隔、6組12起動の計画を実行前に固定した。`python -B D:/NeNeNib/scripts/291-status-layout-tail-20261005.py --prepare` / `--run` / `--analyze`は全て終了0。解析器は正例と5負例で入力不足/余分・提示欠落・非単調時刻・次入力との混在を拒否。各rawはちょうど41入力、暖機も含め全入力が次入力前に提示済みで、上限4096marks未満。全480単打が有効、除外・再試行なし。

| 分布（nearest-rank） | O ms | A ms | 短縮率 |
| --- | ---: | ---: | ---: |
| 中央値 | 1.0250 | 0.6415 | 37.4% |
| p95 | 1.206 | 0.796 | 34.0% |
| p99 | 1.662 | 1.123 | 32.4% |
| 観測最大値 | 1.766 | 1.243 | 29.6% |

起動ごとの中央値と最大値も全6対応組でAが短縮。2ms・5ms・16.7ms超は両版とも0/240であり、遅延件数が減った証拠とはしない。閾値は分布を説明するための数値で、受入幅を置き換えない。文書は空から41文字へ増え、1起動内の値には相関がある。C++のQPCで`input_received`から`frame_presented`までを測り、WM_CHAR受信前の待ちや実画素の発光までの時間は含まない。最大値はこの240回の観測値で、絶対的な最悪時間の保証ではない。

### 同じ計測点による削減箇所の確認

`python -B D:/NeNeNib/scripts/291-render-capsule-diagnostic-20261005.py --output <before/afterの出力先> --prepare <Release metadata>` / `--run` / `--analyze`。元候補に計装を加えた`5476c66`と、新候補に同じ計装を加えた`9c617c0`で各12本、全て有効、終了0。9節目の順序と一意性、隣接8区間の和が単打全体と一致すること、202入力と提示、幾何、既定profileを確認した。解析器の4負例も拒否した。

ステータス区間の中央値は0.282→0.077ms、最大は0.321→0.269ms。本文layout解放は0.001→0.002ms、最大0.002→0.014ms。候補の第6試行は単打1.229msで、status 0.269ms・EndDraw 0.437msなどの増加を含み、そのまま保存した。前後は別時刻の診断群で、本文・EndDraw・Presentの分布も変わっている。通常版の短縮全量を特定区間へ割り当てたり、計装版を通常版の性能受入へ代用したりしない。

### 証拠・再利用・残る範囲

集計と原記録の参照/hashは[`status-layout-capsule-2026-10-05.json`](../status-layout-capsule-2026-10-05.json)。原記録は`D:/NeNeNib/outputs/291-status-layout-normal-20261005`、`291-status-layout-tail-20261005`、`291-status-layout-visual-20261005`、`291-render-capsule-diagnosis-before-20261005`、`291-render-capsule-diagnosis-after-20261005`。事前plan、全rawとtrial、依存源、44画像、測定器、4版のRelease exe/metadata、ビルド記録、4枝のgit bundleを`D:/NeNeNib/evidence/291-status-layout-capsule-20261005/status-layout-capsule-evidence.zip`へ収載した。315原ファイル+manifest、3271724 bytes、SHA-256 `ec9e7369ac0f303bf42d41a9f5a1c6f0a9650a96f50c7b128285a6b746a0228b`。`python -B D:/NeNeNib/scripts/291-status-layout-archive-20261005.py`は終了0、全entry hash・ZIP CRC・bundle verify成功（親`936b50b`が必要）。以前の失敗や証拠束を置換していない。

今回の変更はWin32ステータス描画に閉じるため、core/application全件・無関係な起動/大容量open/IME/splitは再実行しない。文書・集計・commitだけが変わる工程では、検証済み`0dfad49`の実装・試験・依存の成功を再利用する。今回の候補の長行/16MiB性能は未測であり、過去候補の値を今回の実測値にしない。物理DPI遷移、device lost、DirectWrite作成失敗の実再現は未実施。最大7枠の保持メモリと、変更された文字列を組む費用は残る。

性能の総合受入とmain統合は保留。今回の3追加worktree（`291-render-capsule-profile-20261005`、`291-status-layout-capsule-20261005`、`291-status-layout-profile-20261005`）はD側にあり、未統合の診断/実装と測定済みReleaseの参照先なので保持する。完了後は保存済み証拠と稼働参照を確認して整理する。適用は#291 / FR-015 / ADR 0011・0069・0070 / ARC-001・004・005・008・011 / CPP-003・012・016・017 / QLT-001・012・013・014 / GIT-003・004、Waivers: none。

記録の保存では`python -B -`で文書/waiver検査0 findings、追加・変更したローカルリンク9件、原記録5集計の内容/hash一致とZIP hash一致を確認した。`git diff --check`と`git diff --exit-code 0dfad49 -- src tests eng CMakeLists.txt`も終了0。ログは`D:/NeNeNib/outputs/291-status-layout-document-check-20261005.log`。検証後の製品変更はなく、文書commitのためのアプリ再実行はしていない。
