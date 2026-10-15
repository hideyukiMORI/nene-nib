# 5-ck — 長い行の文字組み再利用（Issue #291・ADR 0069）

出典: docs/quality/gate-proofs.md の節 5-ck（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-03〜04。FR-012 / FR-015 / FR-017 / D12 / D14 / D15 / ARC-001 / ARC-004 / ARC-007 / ARC-011 / CPP-011 / CPP-012 / CPP-016 / CPP-017 / QLT-001 / QLT-012 / QLT-013 / QLT-014。hideの続行指示に沿い、単体で原因の計測・修正・表示検証・split再比較を実施した。

### 原因の切り分けと製品の差分

`test/291-render-profile`で既存TimingPort/Milestoneだけに一時計測点を追加し、controller.apply、frame生成、UTF-16、layout作成、DrawTextLayout、EndDraw、待機と提示を分けた。時計はadaptersのQPCだけ。容量は一時的に65536へ増やし、全試行で未到達を確認した。最初の計測版は節目の関数長制限で落ち、未使用の3節目を除いて修正した。抑制と閾値変更はない。

`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`でfedf727、続いてGetMetricsを先行させる比較版a0b2c57を生成し、各々終了0。ログは`out/291-profile-build-2.log` / `out/291-shaping-build.log`、メタデータは`out/release/<SHA>.json`。各版に`python D:/NeNeNib/scripts/291-render-profile.py --release out/release/<SHA>.json`を実行（後者は`--name shaped`）、各6本を固定して終了0。各版は空文書3本＋長い日本語行3本、1280×800 / DPI120 / Cascadia Code13.5pt。全202入力・1文字の独立・節目の組を確認した。

長い行の中央値は325.999ms、19行のDrawTextLayoutが322.712ms。GetMetricsの先行版では文字組みを含むlayout作成が311.463ms、DrawTextLayoutが9.890ms。frame生成0.705ms、待機0.001msに対して、同じ表示行を毎回組み直すことが支配的だった。全段階の試行値は[実測JSON](../long-line-render-2026-10-04.json)のprofileに保存した。

製品の修正はa1a0e4d50a7c9eb784652a4d5ee65e11e7953e0dのui/win32の3ファイル。BodyTextLayoutとDirect2DRenderer.cpp / .hppに、全文・幅・行高が一致する現在/直前のlayoutの再使用と、書式再生成時の破棄を入れた。通常行・IME・クリックは既存layout_ofの一本。文字の範囲、fallback、描画API、編集状態、schemaは変更しない。計測点・先行GetMetrics・2枠試作は製品へ入れない。

### 対象にした表示の検証

退行の対象は、再使用によって古い文字・位置・書式が残ることと、共有layoutに選択等の表示が混入すること。`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`はa1a0e4dで終了0（`out/291-cache-build.log`）、警告とclang-tidyは既存設定。1307648 bytes、SHA-256 `20BDE40B142DCC16409E5CE4973B1FBA91B5FB739FB474835BDD35A88CF0FC4D`。

変更3ファイルへclang-formatのdry-run、conformance.source_checksを選び、ADR 0069へdocument_checksを実行して違反0。型の配置・禁止API・抑制・文書契約を確認するためで、無関係なcore/applicationの全件テストは実行しない。

`python D:/NeNeNib/scripts/291-layout-visual.py --executable <Release> --name before|after`を591b99fとa1a0e4dへ実行し、`--compare`は終了0。22場面の本文/ステータス全画素が一致（`out/291-visual/comparison.json`）。Tab、結合文字、異体字、絵文字、制御文字、双方向文字を含むfixtureで、再描画・入力・Backspace・改行・undo・3か所のクリック・選択・スクロール往復・窓幅往復・Vimのブロックキャレット・検索・フォントサイズ/名・テーマを確認した。刺激で画像が変わることと、設定の実保存値も検査した。初回の検証スクリプトはguifontの`:h<pt>`を欠き設定変更を検出できず停止したため、スクリプトだけを修正して再実施した。失敗した記録はbefore-invalid-fontへ保存した。

既存`eng/verify-window.py`のverify_imeだけを専用profileで呼び、終了0（`out/291-visual/ime/result.json`）。日本語IMEの実入力で下線189画素、変換後の強調186画素、確定後の下線0を確認し、IMEの開閉状態を元の0へ戻した。DPIは実機120で、別DPIへの実移動とdevice lostの実機再現はしていない。フォント変更と同じ資源破棄へ通るコードを確認した。

### 通常版とsplitの固定比較

Oは変更前591b99f、Aは修正通常版a1a0e4d、S/H/Vは4d60d79 / 58a48fb / fc0b4c0。試作は#290と同じ矩形・クリップ・同じframeの二度描画で、各Refをbuild-release.ps1で生成して終了0。Sは143.923秒、Hは176.354秒、Vは178.419秒。H/Vは隔離したworktreeから生成した。メタデータと全hashは実測JSONに保存し、ビルドを全て終了してから測った。

測定前にIssueへ3本文×6組×5版＝90本、奇数O/A/S/H/V・偶数V/H/S/A/O、欠測は記録して再試行なし、O/Aの前半後半の安定を要する通常版比較、#290と同じsplit条件を記録した。
`python D:/NeNeNib/scripts/291-paired-render.py --manifest D:/NeNeNib/evidence/291-paired/manifest.json`は終了0。通常版の改善とsplitを一度に測り、別々の測定を重複させなかった。窓は1280×800、DPI120、各回新規profile、fixtureと字体は#290と同じ。測定器は既存keys_trialを共用した。

`python D:/NeNeNib/scripts/291-render-audit.py`は終了0（out/291-render-audit.log）。90試行のうち87本は正確な202入力、暖機/1文字の独立、200文字後の提示、rawからの時間再計算、寸法/DPIを検証した。empty-3-O、empty-3-S、long-japanese-2-Oは207入力だったため欠測として保持した。値の補完・試行の置き換え・再試行はない。全て測れている修正通常版Aは3条件とも6本。

観測された1文字 / 200文字の中央値（ms）。Oの空/長行とSの空は5本、他は6本。欠測がある数値も見せるが、性能受入の合格値とはしない。

| 本文 | O 変更前 | A 修正版 | S 1枠試作 | H 上下 | V 左右 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 空文書 | 0.893 / 3.708 | 0.852 / 3.339 | 0.798 / 3.277 | 0.914 / 4.094 | 0.830 / 3.393 |
| 16MiB短行 | 1.770 / 9.800 | 1.171 / 9.520 | 1.140 / 8.564 | 1.157 / 8.704 | 1.720 / 8.992 |
| 長い日本語行 | 333.784 / 485.280 | 28.616 / 186.354 | 28.941 / 194.542 | 28.309 / 182.767 | 38.173 / 198.512 |

長行の1文字の観測中央値は91.43%小さく、対応が取れた5組は全て292〜321ms短縮した。200文字も同じ5組が全て285〜314ms短縮した。ただし通常版の厳格比較は、空/長行のOに欠測、短行のOの200文字に前半11.005ms→後半9.219msの揺れ1.786ms（許容0.980ms）があり、3条件とも保留。成功したAの18本を失敗扱いにはしないが、O/Aの安定が必要という事前条件を外して合格にしない。

splitはAの6本が全条件で安定していた。空のH/Vと短行のS/Hは事前条件内。空のSは欠測、短行のVは1文字追加0.549msが0.5msを越える。長行のSは200文字追加8.188msが許容18.6354ms以内でも5/6で増えたため、少差の一貫した増加を保留する条件に該当。H/Vの1文字の合計28.309 / 38.173msは2ms条件外で、Vの200文字追加12.1575msも1ms条件外。splitの製品採用と文書/枠の所有移行は引き続き保留する。同じ文字列を両枠へ出すため、cacheを共有できる有利な条件であり、独立した文書の費用は未測。

### 証拠、自己レビューと受入状態

profile.bundleとsplit-recheck.bundleはgit bundle verifyで成功し、共にmainの936b50bを必要な親として保存した。後者は修正元a1a0e4dも含むため、試作枝を消してもソースを復元できる。各試作の差分も保存した。
全235ファイルの束は`D:/NeNeNib/evidence/291-render/long-line-render-evidence.zip`、9082031 bytes、SHA-256 `6e1fe550cf41204aa58a3e739ff39ede1eae2fb384833f89e4f870bd239397e1`。ZIPのCRC検査は成功。exeは含めず、全Releaseの生成元とhash、fixture、raw時刻、画像、ソースbundle、測定器を含む。

自己レビュー: 本文/履歴を複製せず描画資源だけをuiに置く（ARC-001 / 004 / 011）。時刻と追加の非決定的入力を製品へ足さない（ARC-007）。COMはComPtr、移した空のlayoutは一致対象から除外し、フォント名/サイズ/DPIの書式再生成とframe終端で破棄する（CPP-016）。全ての本文の描画/IME/hit testがlayout_ofに通り、選択/検索の処理は共有layoutを書き換えない。CPP-011 / 012は型の配置と通常Releaseの静的解析で確認した。

90試行の時点では実装・表示検証済み、性能受入とmergeは保留。draft PRで差分と証拠を保持した。ゲート/既存速度基準値/schemaの変更なし、Waivers: none。00:40にhideから、測定中にキーボードを操作していたとの回答を得た。予定外の入力があった3本は欠測のまま保存した。

文書の整合はconformance.document_checksで違反0（`out/291-doc-conformance.log`）、`git diff --check`は終了0。`git diff --exit-code a1a0e4d -- src tests eng CMakeLists.txt`も終了0で、文書をまとめた後も検証対象の実装と依存は変わっていない。

再利用: a1a0e4d以降の変更は文書だけで、製品の3ファイル・対象検証・関連依存は不変。22場面の一致、IME、Releaseと今回の測定をPR工程だけの理由で繰り返さない。残るリスクは、編集した長い行と初めて見える行の全文字組み、frame生成、同じ描画の再実行、表示行ぶんの資源保持、DPI/device lostの実機未確認、および欠測/対照揺れによる厳格比較の未判定。

### 手動入力のない時間に行った通常版36本の別比較

00:47にIssueへ別計画を先に保存し、hideの「いいよ」を受けて00:53〜00:57に実施。O/Aだけを3本文×6組＝36本、奇数O/A・偶数A/Oの順に固定した。以前の90本は上書き・補完せず、バイナリ、fixture、窓、DPI、字体、keys_trialと判定条件は同じ。表示22場面、IME、Release、split比較は再利用して繰り返していない。

`python D:/NeNeNib/scripts/291-normal-paired.py --manifest D:/NeNeNib/evidence/291-normal-paired/manifest.json`は終了0。続く`python D:/NeNeNib/scripts/291-normal-audit.py`も終了0（`out/291-normal-audit.log`）。36本全て正確な202入力で欠測なし。暖機/1文字の独立、raw時刻からの再計算、窓1280×800 / DPI120も一致した。実測JSONのnormalRecomparisonに原記録と監査を追加した。

| 本文 | O 変更前 1文字 / 200文字 ms | A 修正版 1文字 / 200文字 ms | 事前条件での判定 |
| --- | ---: | ---: | --- |
| 空文書 | 0.8895 / 3.5225 | 0.8095 / 3.8920 | Aの200文字が前半/後半で不安定のため保留 |
| 16MiB短行 | 1.8190 / 10.1640 | 1.4485 / 8.7405 | O/Aとも安定、受入条件内 |
| 長い日本語行 | 333.9125 / 498.3855 | 30.2085 / 194.2555 | O/Aとも安定、受入条件内 |

長行の1文字は90.95%、200文字は61.02%短縮し、両方とも対応する6組全てで短縮した。短行も1文字6/6、200文字5/6で短縮した。
空文書は中央値の増分が1文字-0.0800ms、200文字+0.3695msで許容内だが、Aの200文字の前半4.494ms→後半3.256msの差1.238msが許容0.500msを超えた。Oの200文字は前半3.537ms→後半3.508msで安定。今回も空文書全体を保留とし、中央値だけで合格にしない。入力混入による欠測は解消したが、この揺れの原因は未特定であり、退行確定とも無関係な測定ノイズとも断定しない。追加の再試行は行わない。

別比較の束は`D:/NeNeNib/evidence/291-normal-paired/normal-render-evidence.zip`、57ファイル、1105344 bytes、SHA-256 `73580c19e1ec17ed7a1c7b1b7cc6a654e267dbc51c706a65c3f69c3ceb5547e5`。CRC検査成功。36本のraw、画像、fixture、測定器、事前計画、Releaseメタデータを含み、ソースは元の235ファイルの束をhashで参照する。

**現在の残件は空文書の連続入力の安定性。** [PR #293](https://github.com/hideyukiMORI/nene-nib/pull/293)はdraftを維持し、性能受入全体とmergeは保留。splitの採用保留も変わらない。成功した短行・長行と表示確認は保存して再利用し、原因を特定せず合格まで測り直すことはしない。基準値・閾値・schemaは変更せず、waiverなし。

追記後は文書の規則参照とPRの検証記録だけを確認した。document_checksは違反0（`out/291-normal-doc-conformance.log`）、`python eng/git-conventions.py D:/NeNeNib/briefs/pr-291-long-line-rendering.md --pr-body`と`git diff --check`は終了0。製品・対象テスト・依存はa1a0e4dから変わらず、アプリの再検証は行っていない。

### 並行作業がある環境での空文書の内訳診断

hideから他の作業が並行しGPUも使われているとの説明と続行指示を受けた。01:14にIssueへ固定12試行の診断計画を先に保存し、他プロセスや設定を変えずに実施した。共有負荷はhideからの説明であり、GPU占有率を別途計測したものではない。

変更前の計装版fedf727を再利用し、同じ計測点だけをa1a0e4dへ重ねた2d8a10eを`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`で生成、終了0（`out/291-empty-profile-build.log`）。151.036秒、1308160 bytes、SHA-256 `88F95A6BE15095518474CDAF659C74781EE10F948008DF5C5115F4B04A8476DC`。2版の製品コードの追加/削除行が、検証済みのcache修正だけであることも比較した。最終的な製品枝には計装を入れない。GetMetrics先行版は使っていない。

`python D:/NeNeNib/scripts/291-empty-diagnostic.py`を01:19〜01:20に実行して終了0。空文書6組、奇数O/A・偶数A/Oの12本は全て202入力、欠測なし。単打の独立、全節目の対、rawからの時間再計算、1280×800 / DPI120を確認した。節目数はOが709、Aが706で容量65536に未到達。集計処理は先に保存済みのprofileのrawでも確認した。単打・200文字全体・最後の入力以降を分解し、重なるUTF変換/文字組み/描画の区間を二重加算していない。各区間はQPCの経過時間で、CPUの実使用時間ではない。

最後の入力から提示までに含まれる各区間の中央値（ms）。各行を別々に中央値へ取っているため合計は一致しない。

| 区間 | O 変更前 | A 修正版 |
| --- | ---: | ---: |
| 本文描画 | 0.2495 | 0.1460 |
| EndDraw | 0.3460 | 0.3165 |
| 交換鎖の待機 | 0.0010 | 0.0010 |
| Presentを含む提示 | 0.0480 | 0.0595 |
| 最後の入力から提示まで全体 | 1.0275 | 0.9565 |

本文描画は単打・連続入力とも対応する6組全てで短縮した。修正版の連続入力後の全体は0.929〜1.039msで、前回の1.536〜2.456msの跳ねは再現していない。修正版の第6組の単打だけは1.266msで、Presentを含む区間0.312msやstatus描画を含む区間0.404msなど複数箇所が長かったが、これを前回の連続入力と同一原因とは扱わない。

**追加の製品修正を入れる根拠は得られなかった。** 今回の修正部分は短縮したが、前回の揺れの原因は未特定。共有GPU負荷が原因だとも、この診断で通常版の空文書の受入が済んだとも主張しない。PR #293はdraftのまま、共有負荷が落ち着いた時の通常版の空文書確認を残件にする。12本を繰り返さず、短行・長行・表示・IME・splitは再利用する。

全記録は実測JSONのemptyDiagnostic。束は`D:/NeNeNib/evidence/291-empty-diagnostic/empty-diagnostic-evidence.zip`、27ファイル、106822 bytes、SHA-256 `4f148c785fe116bae1f3ca454ff802aebefee76cc16ad50495a79db05a547112`、CRC成功。raw、画像、事前計画、測定器、Releaseメタデータ、検証済みgit bundleと差分を含む。製品コードはa1a0e4dのまま、受入条件・schemaは変更せず、waiverなし。

文書追記後のdocument_checksは違反0（`out/291-empty-diagnostic-doc-conformance.log`）、PR本文のgit-conventions.pyと`git diff --check`は終了0。`git diff --exit-code dcf5811 -- src tests eng CMakeLists.txt`も終了0で、製品枝に計装が混入していない。文書のための製品テストは追加していない。

2026-10-04 02時台、hideの区切りの依頼で日報と引き継ぎ書を整理した。対象はCLAUDEの最新リンク、reports / handoffs / todoと本記録。リンク・規則参照の退行だけをdocument_checksで確認し、違反0（`out/291-handoff-doc-conformance.log`）。PR本文のgit-conventions.py、`git diff --check`、`git diff --exit-code 5e96aa7 -- src tests eng CMakeLists.txt`も終了0。製品・対象テスト・依存は不変で、ビルド・アプリ検証は再利用した。PR #293はdraft、空文書の通常版確認を残して区切る。

### 空文書限定の測定準備（2026-10-04夕方、実測なし）

hideの続行・分担指示により、設計サナが既存の状態・受入条件・コード経路をレビューし、SOLの実装サナがrunnerと監査を準備した。その後hideから「今は操作中。測定の準備まで進めて」と指示があり、窓の起動、前面操作、入力、実測を一切行っていない。Issue #291に固定計画を追記し、PR #293はdraftを維持する。

対象は未判定の通常版の空文書のみ。O=591b99f / A=a1a0e4d、6組12本、奇数O/A・偶数A/O、1280×800 / DPI120、設定ファイルのない新規profileで既定Cascadia Code13.5pt。既存`eng/measure-speed.py.keys_trial`を各試行1回だけ呼ぶ。202入力と暖機/単打の独立、200文字後の提示、rawから再計算した時間、既存50ms以内の到着幅を監査する。寸法/DPIは時刻raw自体には無く、入力前と閉じる直前の`observation.json`で照合する。

受入条件は前計画を維持する。O/A各6本が揃い、各版の前半3本/後半3本の中央値差が単打max(0.1ms,その版の全6中央値の10%)、200文字max(0.5ms,同10%)以内で、A-Oの中央値差もO基準の同許容幅以内のときだけaccepted。欠測・不安定はholdとし、追加試行・上書き・合格までの反復を拒否する。GPU負荷の有無は開始時にhideと確認し、他の作業を停止しない。

新しい道具だけを対象に、`python D:/NeNeNib/scripts/291-empty-acceptance.py --prepare`、`python D:/NeNeNib/scripts/291-empty-acceptance-audit.py --self-check`を実行し、両方終了0。合成rawの正例と、未完了JSON・提示欠落・入力不足・単打の独立性欠如、判定の安定した正例と不安定・許容差超過・欠測・実際に順序を入れ替えた反例を確認した。これは監査器の動作確認であり、製品性能の受入ではない。結果は出力領域の`self-check.json`。設計レビューで設定ファイルを書かない従来条件と、過去の準備版を実行領域に混ぜないことを確認した。

実行領域は`D:/NeNeNib/outputs/291-empty-acceptance-20261004-evening/`で、現在は`manifest.json` / `plan.json` / `self-check.json`のみ。開始記録・試行profile・rawは無い。runner SHA-256は`6b8226fdfbc6a4f0e1082f5abafd5278b9a92b52858dd58339dff983df509e10`、監査は`9f382496c5906ec9a91f19ab8c9ff725e0c2c1d405d85edf44cc04e63998c8ff`、固定planは`b51905ea80ee7086f1dadcafc969fed634edc1fb154e9a0d616033c7ae84e6ef`。自己確認が参照したコードと現行コード、plan内の依存hashが全て一致することを親でも確認した。

準備の証拠は`D:/NeNeNib/evidence/291-empty-acceptance-preparation/preparation-evidence.zip`。16ファイル、41561 bytes、SHA-256 `49bee5c0b1ba378875bd58aab19e9b9bede62aba814fc2a791e7fef83201815c`、ZIPのCRC成功。runner・監査・現行plan/manifest/自己確認・置き換えた未実行の準備記録・事前計画・Releaseメタデータ・共用測定器を収載した。実測を含まない束として区別する。

再利用の根拠: `Get-FileHash -Algorithm SHA256 build/release-a1a0e4d/NeNeNib.exe,build/release-591b99f/NeNeNib.exe`で既存metadataと一致、`git diff --exit-code a1a0e4d HEAD -- src tests eng CMakeLists.txt`も終了0。成功済みのRelease、表示22場面、IME、短行・長行、split比較を再実行しない。製品・schema・性能閾値の変更なし。適用はQLT-001 / QLT-012 / QLT-013 / QLT-014 / CNF-006 / GIT-004、Waivers: none。空文書の性能受入とmerge、splitの採用は未完了のまま。

準備の文書更新では、リンク・規則参照の退行だけを`conformance.document_checks(root, inventory(root), rules)`で確認し、違反0（`out/291-empty-prepare-doc-conformance.log`）。PR本文は`python eng/git-conventions.py D:/NeNeNib/briefs/pr-291-empty-prepared.md --pr-body`が終了0、`git diff --check`も終了0。製品テストは追加していない。追加worktreeは無く、未実施の固定計画を持つ出力と、未実行の準備履歴を持つ`D:/NeNeNib/outputs/291-empty-acceptance-preparation-history-20261004-evening/`を残す。#291の受理・統合後に証拠の収載を確認して整理し、ブランチとcommitは保持する。

### 通常版の空文書12本の結果（2026-10-04 22時台）

hideの「今作業してるエージェントが少ないからテストしてみて」を受け、夕方にIssueへ登録した固定計画を22:01:28〜22:02:50に一度だけ実行した。22:02:52に前面操作の終了を伝えた。稼働エージェントが少ないという申告は記録するが、GPU占有率や他プロセス負荷は測っておらず、無負荷環境とは扱わない。他の作業の停止・追加ビルドは行っていない。

`python -u D:/NeNeNib/scripts/291-empty-acceptance.py --run`は終了0（`out/291-empty-acceptance-run.log`）。通常版O=591b99f / A=a1a0e4dの6組12本は全て有効。`python D:/NeNeNib/scripts/291-empty-acceptance-audit.py`は終了2（`out/291-empty-acceptance-audit.log`）、理由は欠測ではなく事前の安定条件未達。全12本で202入力・暖機/単打の独立・200文字後の提示・時刻rawからの再計算・到着幅・入力前/終了前の1280×800 / DPI120・新規profileの設定ファイル不在を確認した。planの依存hashとReleaseは不変。追加試行は0。

| 指標 | O 変更前の中央値 ms | A 修正版の中央値 ms | A-O ms | O基準の許容幅 ms |
| --- | ---: | ---: | ---: | ---: |
| 1文字 | 1.0515 | 1.0470 | -0.0045 | 0.10515 |
| 200文字 | 3.6825 | 3.4670 | -0.2155 | 0.50000 |

中央値の差は両方とも許容内だが、両版の安定性を必要とする条件を満たさない。

| 対象 | 前半3本中央値 ms | 後半3本中央値 ms | 絶対差 ms | 許容幅 ms | 判定 |
| --- | ---: | ---: | ---: | ---: | --- |
| O 1文字 | 0.999 | 1.081 | 0.082 | 0.10515 | 安定 |
| O 200文字 | 4.202 | 3.638 | 0.564 | 0.50000 | 不安定 |
| A 1文字 | 1.122 | 0.876 | 0.246 | 0.10470 | 不安定 |
| A 200文字 | 3.347 | 3.656 | 0.309 | 0.50000 | 安定 |

したがって通常版の空文書は引き続きhold。修正版の中央値悪化や入力欠落を検出したという結果ではなく、対照も含む揺れにより性能比較の受入を確定できない。揺れの原因は未特定で、GPU・他作業・製品のいずれとも断定しない。現在の条件を緩めず、同じ試行を繰り返さない。

SOLの実装サナが読み取り専用で独立に再計算し、12本の入力・提示・寸法/DPI・profile・hashとhold判定が一致。200文字の途中の提示は全12本0枚。到着幅の前半/後半中央値はO 2.960/2.594ms、A 2.369/2.675ms、最後の入力→提示はO 1.130/1.046ms、A 0.963/0.981msだった。中央値の分解を足して原因の寄与率とは扱わない。報告は`D:/NeNeNib/outputs/291-empty-acceptance-review/review.txt`、SHA-256 `d1af1fc83e99d47a4e3c29c193e92d17820fd58aa16de12db7e9eea74f8e6800`。レビューでGUI・再測定・ビルドは行っていない。

原記録と監査は実測JSONの`emptyAcceptance`へ追加し、既存の全sectionを値比較で不変と確認した。証拠の束は`D:/NeNeNib/evidence/291-empty-acceptance/empty-acceptance-evidence.zip`、65ファイル、67478 bytes、SHA-256 `c5eeb7923c1acd4455d827d160028baf45c3dbb557b43fc6821eb584b268498b`、CRC成功。全12本のraw/observation/profile、plan、runner/監査、共用測定器、Releaseメタデータ、ログ、実行時の申告、独立レビューを含み、exeは含めない。旧90本・36本・計装診断12本・準備の束は置換しない。

製品はa1a0e4dのまま。成功済みのRelease・表示22場面・IME・短行・長行・split比較を再利用し、今回の未達で無関係な検証を追加しない。PR #293はdraft、mergeとsplit採用は保留。schema・基準値・受入条件の変更なし。適用はADR 0069 / QLT-001 / QLT-012 / QLT-013 / QLT-014、Waivers: none。追加worktreeは無く、未受理のrawを持つ実行領域と独立レビュー出力は#291の受理・証拠収載後の整理まで保持する。

記録更新の検証は文書参照とJSONの追加範囲に限定した。document_checksは違反0（`out/291-empty-result-doc-conformance.log`）、`python eng/git-conventions.py D:/NeNeNib/briefs/pr-291-empty-result.md --pr-body`と`git diff --check`は終了0。実測JSONは更新前のHEADと全既存sectionを値比較して一致し、追加はemptyAcceptanceだけ、12本有効とholdをそのまま保持している。

### hideの追加指示による通常版の空文書12本（2026-10-04 22:15）

hideの「もう一回やってみて」を受け、先のholdと原記録を残したまま、同条件の別計画をIssueへ先に記録した。環境を改善したと確認できたための再試行ではない。runnerはOUTPUTとAUDITORの参照先2行だけを変更し、監査器は同一。`D:/NeNeNib/scripts/291-empty-repeat-20261004-2214/291-empty-acceptance.py --prepare`と同所の監査器`--self-check`は終了0。

`python -u D:/NeNeNib/scripts/291-empty-repeat-20261004-2214/291-empty-acceptance.py --run`を22:15:31〜22:16:52に実行し終了0。同所の`291-empty-acceptance-audit.py`は終了2。全12本の入力・提示・寸法/DPI・profile・hashは有効だが、4条件全てで前後半の安定条件を満たさずhold。固定計画内の追加試行は0。22:16:53に前面操作の終了を伝えた。

| 指標 | O 変更前の中央値 ms | A 修正版の中央値 ms | A-O ms | O基準の許容幅 ms |
| --- | ---: | ---: | ---: | ---: |
| 1文字 | 1.1290 | 1.0145 | -0.1145 | 0.11290 |
| 200文字 | 4.4215 | 4.0865 | -0.3350 | 0.50000 |

| 対象 | 前半3本中央値 ms | 後半3本中央値 ms | 絶対差 ms | 許容幅 ms |
| --- | ---: | ---: | ---: | ---: |
| O 1文字 | 1.188 | 1.070 | 0.118 | 0.11290 |
| A 1文字 | 1.084 | 0.859 | 0.225 | 0.10145 |
| O 200文字 | 3.641 | 4.450 | 0.809 | 0.50000 |
| A 200文字 | 3.717 | 4.471 | 0.754 | 0.50000 |

SOLの実装サナの独立再計算でも、全rawの有効性とholdが一致。報告は`D:/NeNeNib/outputs/291-empty-repeat-review-2214/review.txt`、SHA-256 `76d5cf09b2f37b9316b69a89d54317d2f169a188f039b27a20c0c6bd791133c1`。実測JSONの`emptyAcceptanceRepeat`へ別sectionで収載し、前の結果を置き換えていない。

証拠の束は`D:/NeNeNib/evidence/291-empty-repeat-2214/empty-repeat-evidence.zip`、65ファイル、65782 bytes、SHA-256 `3a56d4071794e28f53e12b9aad830ef7fc17868d6a4348cb2828e819c67bd4d4`、CRC成功。raw・監査・plan・測定器・実行時の申告・Releaseメタデータ・独立レビューを保持し、exeは含めない。出力`D:/NeNeNib/outputs/291-empty-repeat-20261004-2214/`と独立レビューは未受理の証拠として保持する。

この比較でも製品退行やGPU起因は確定していない。製品はa1a0e4dのまま、PR #293はdraft。成功済みの関連検証は再利用し、受入条件・基準値・schemaは変更していない。適用はADR 0069 / QLT-001 / QLT-012 / QLT-013 / QLT-014、Waivers: none。

記録更新はdocument_checksが違反0（`D:/NeNeNib/outputs/291-empty-repeat-20261004-2214/doc-check.txt`）、PR本文の`python eng/git-conventions.py D:/NeNeNib/briefs/pr-291-repeat-result.md --pr-body`が終了0。実測JSONの既存sectionを更新前HEADと値比較して全て一致し、追加はemptyAcceptanceRepeatだけ。`git diff --exit-code a1a0e4d HEAD -- src tests eng CMakeLists.txt`も終了0で、記録のための製品テストを追加していない。

### 揺れの所在を調べる内訳診断とOS追跡の制約（2026-10-04 23時台）

hideの原因調査指示と「進めて」を受け、保存済み通常raw24本の分解・コード追跡に続いて、既存計装O=fedf727 / A=2d8a10eの固定6組12本を実施した。事前planと依存hashは`D:/NeNeNib/outputs/291-empty-stage-investigation-20261004/plan.json`、SHA-256 `69e8bf6758fbdc8edc14655f99ae2b6f1ab4e7ba466bb5c71b40cab5aebafc6b`。旧stage解析を変更せず再利用し、`--prepare`と既存raw再解析で一致を確認してから開始した。

`python -B -u D:/NeNeNib/scripts/291-empty-stage-investigation-20261004.py --run --plan-sha256 69e8bf6758fbdc8edc14655f99ae2b6f1ab4e7ba466bb5c71b40cab5aebafc6b`は23:11:52〜23:13:13、終了0（`D:/NeNeNib/outputs/291-empty-investigation-20261004/stage-run.log`）。全12本で202入力・暖機/単打の独立・提示・段階マークの対・1280×800 / DPI120・設定不在を確認。欠測0、追加試行0。23:13:13に前面操作終了を通知した。計装版の経過時間であり、通常版の性能受入へ代用しない。

O第2組の200文字は4.830ms、そのうち最初〜最後の入力受信が3.816ms。199入力間隔の内訳は、input→apply開始の合計0.020ms、apply内1.411ms、apply終了→次input2.385ms。最後の区間はほかのO5本で1.121〜1.148msだった。100µs以上の入力間隔が5か所あり、最大220µsは第53→54文字（apply6µs / apply後214µs）。単一の約1ms停止ではなく、主な増加がapply後へ分散していた。

apply後にはIME方針反映・再描画要求・題名更新判定・告知/終了判定・frameや一時文字列の解放・メッセージループ・OSの実行待ちが含まれる。controller.apply内のframe生成はapply側に計上済み。この入力経路はO/Aで同じで、途中に本文cacheを実行する経路はない。ただしOS待ちと未計装のCPU処理はまだ区別できない。

O第1組の単打1.863msではEndDrawが1.108ms（ほかのOは0.180〜0.277ms）だった。API内部のCPU処理・driver処理・GPU待ち・OSの実行待ちはrawだけでは識別できず、全ての単打の揺れが同じ箇所に出たわけでもない。測定した単打/連続入力の交換鎖Waitは0〜0.003msで、この待機の長さは今回の大きな増加を説明しない。

本文bodyは単打・連続入力とも全6対応組でAが短縮。中央値は単打O/A=0.2445/0.1185ms、連続入力後O/A=0.2600/0.1395ms。**今回のcacheを揺れの原因と判断する証拠は得られなかった。OSコンディションは有力な候補だが未確定**。独立再計算でも全raw/hash/段階/summaryと解釈が一致。報告は`D:/NeNeNib/outputs/291-empty-investigation-20261004/stage-review.txt`、SHA-256 `e01795a77cfdc3da3b5fdb0dd63d1b5cb3e5276f9441673fd84e74eefcc768b7`。

hideのOS条件についての問いを受け、診断専用のWPR設定`D:/NeNeNib/scripts/291-os-waits.wprp`を準備。単一32MiB collectorでCSwitch/ReadyThread/ProcessThread/ThreadPriority/Loader/CpuConfig/Power/DPC/Interruptのみを記録する設定は`wpr -profiledetails`終了0。独自`NeNeNib291Capability`のstartを1回だけ試み、`0x80070005 Access is denied`で拒否された。直後のstatusは独自session不在、ETL未取得。現在の権限ではOS実行状態の裏付けを取得できない。他session操作・権限変更・OS設定変更・再試行は行っていない。詳細は`D:/NeNeNib/outputs/291-os-wait-trace-preparation-20261004/capability-result.json`。これは原因診断のための外部観測であり、ADR 0011の正典の性能測定をETWへ置換するものではない。

実測JSONの`emptyStageInvestigation`へ別sectionで保存。証拠の束は`D:/NeNeNib/evidence/291-empty-stage-investigation-20261004/empty-stage-investigation-evidence.zip`、66ファイル、348776 bytes、SHA-256 `78d37a60b4a53742e4b17ed493110f749ae947a12df885c59c606003ad5e48a9`、CRC成功。raw/plan/trial/profile/測定器/Release metadata/通常rawと入力間隔の分解/独立レビュー/OS追跡の拒否記録と、旧計装・通常比較2回の証拠の束を含む。exeは含めない。

製品はa1a0e4dのまま、追加の修正・ビルドは行っていない。通常版のholdとdraft PR #293を維持。成功済みRelease・表示・IME・短行・長行を再利用し、受入条件・基準値・schemaは不変。適用はADR 0011/0069、QLT-001/012/013/014、ARC-007、Waivers: none。追加worktreeなし。未解決の調査資料と未受理の証拠をD領域に保持する。


### 入力後段の切り分けと重複無効化の改善（2026-10-04 23:27以降）

hideの「30分くらい放置するので自由にテスト・検証・改善」の指示で、設計サナが判断・計測・受理を担当し、既に指名されたSOLの実装サナが隔離した計装/通常版候補と読み取りレビューを担当した。前面操作は23:52:08に終了を伝えた。他プロセスの停止、OS設定や権限の変更は行っていない。

まず通常の入力経路と同じ計装Aへ入力後段6節目を足し、固定12本を23:37:32〜23:38:57に実行した。全202入力の順序、199区間の分解、途中描画/提示なし、1280×800・DPI120、新規profileの設定不在、raw/依存hashを確認し、欠測0。199区間の合計の中央値はapply 1.4745ms、invalidate 0.5785ms、messageLoop 0.7090ms。第12試行はburst 6.373msのうちarrival 5.448ms、tail 0.925msで、invalidate 1.317ms・messageLoop 2.309msが増えた。境界elapsedには隣接処理や計測費用が含まれ、messageLoopをOS待ち時間そのものとは扱わない。WPR権限拒否を迂回していない。

改善は`EditorWindow.cpp/.hpp`の2ファイルに限定した。同じ`invalidate()`入口で、全クライアント領域の`InvalidateRect`が成功したときだけ窓の`paint_pending_`を立て、次の`WM_PAINT`まで重複API要求を省く。`paint()`は常に実行し、`ValidateRect`直後・描画前に解除して再入による次要求を残す。失敗時は次の変更で再要求できる。本文/編集状態の所有やWM_PAINT集約は不変で、ADR 0011の決定6へ実装詳細を追記した。根拠となる契約は[InvalidateRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-invalidaterect)と[ValidateRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-validaterect)。

計装版A=`4d8040a`、同じ計測点で重複を省くB=`790e1bd`を固定6組12本（奇数A/B・偶数B/A）で比較した。欠測0、全試行202入力、途中描画/提示0。invalidate区間合計の中央値は0.6555→0.0190msで、対応6組すべて短縮。burst全体は4.438→3.567ms、短縮4/6組。singleは1.0520→1.0635ms、短縮3/6組。重複処理の軽減は確認できるが、全体が安定して改善したとは言わない。B第2組のmessageLoopは2.217msで、揺れは残った。

通常版候補は`a1fd1e16440352111f37797874d0e8029df9276f`、Release SHA-256 `FAEA73967345B4AD655E26AE68DD45506DAECD9A07A815F28FB8651163955D83`、1307648 bytes。計装点は含まない。O=`591b99f`対A=この候補を新しい固定6組12本で比較し、既存runner/監査の入力・幾何観測・判定式をそのまま再利用した。旧A=`a1a0e4d`との比較ではない。全12本有効だが**性能受入はhold**。

| 指標 | O 中央値 ms | 候補A 中央値 ms | A-O ms | O基準の許容幅 ms |
| --- | ---: | ---: | ---: | ---: |
| 1文字 | 0.9685 | 1.1080 | +0.1395 | 0.1000 |
| 200文字 | 4.4880 | 3.4830 | -1.0050 | 0.5000 |

O単打は前半0.962→後半1.238ms、差0.276>許容0.1ms。A連続入力は前半3.033→後半3.627ms、差0.594>許容0.5ms。さらに単打の中央値差も許容を超える。Oも不安定なので恒常的な製品退行やOS原因は確定せず、有利なburst中央値だけで合格にしない。各固定計画内の追加試行は0で、過去のholdと全rawを保持する。

| 検証コマンドと対象 | 確認する退行・結果 |
| --- | --- |
| 各D worktreeで`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`、変更ファイルのclang-format、`git diff --check` | enum/入力経路の警告・型・整形。最初の計装`322924c`はmilestone_name 71行でclang-tidy失敗。不要な本文内部6節目を除いた`4d8040a`、`790e1bd`、通常`a1fd1e1`は終了0。抑制・閾値変更なし。各成功版は1回生成 |
| `python -B D:/NeNeNib/scripts/291-post-apply-diagnostic.py --self-check`、`--prepare`、`--run --plan-sha256 c21ad1776b2a5f95a250157f8ca95f80c8489f421c9f0234a8f0b54dee807a0b` | 境界解析の正例、欠落・順序誤り負例と実測12本。終了0。旧本文内部4段階は未計測と明記し0埋めしない |
| `python -B D:/NeNeNib/scripts/291-coalesce-paired.py --prepare --a <4d8040a.json> --b <790e1bd.json>`、`--run --plan-sha256 441fe056f31961beed88a22164346a02708af91fdd3c73dc98beade039f4fb46` | 同じ計測点で重複要求区間の比較。終了0・12本有効、前述の数値。raw/依存/集計の独立再計算も一致 |
| `python -B D:/NeNeNib/scripts/291-coalesce-visual.py --exe <各exe> --name A` / `--name B` / `--compare` | 無効化の抑止や解除漏れ。両版正常終了、12場面の本文/ステータス全画素一致。編集→次paint編集、外部再描画→編集、縮小/拡大、最小化復帰→編集、モード、字体サイズ、テーマを確認。独立した刺激が画に出たことも検査。代表3画像を目視 |
| `python -B D:/NeNeNib/scripts/291-coalesce-normal-acceptance.py --prepare` / `--run` / `--audit` | 計装なしの通常性能。準備/自己確認/実測は終了0。監査は非0（hold、外側PowerShellの終了値1）。全12有効、安定条件と単打数値条件未達。失敗を隠す再実行はしない |
| `python -B D:/NeNeNib/scripts/291-coalesce-ime.py` | 共通invalidateを通るIME変換中/確定の描画漏れ。既存verify_imeを1回呼び、実鍵で変換・Space・Enter、確定後下線0、通常/Vim NORMAL/INSERTの開閉、開始時IME状態への復元、正常終了。終了0 |
| `git diff --exit-code a1fd1e16440352111f37797874d0e8029df9276f -- src tests eng CMakeLists.txt` | 作業枝へ取り込んだソースと検証済み通常候補の同一性。終了0。文書・commit変更だけでビルド/GUIを再実行しない |

旧layout-only版の22場面、短行・長行の数値、split試作は歴史的な成功記録として保持。今回の候補では共通入力経路が変わるため、旧短行・長行の性能値を新候補の実測値とは呼ばない。今回追加した変更には上の12場面とIME、空文書固定比較を対応させた。core/application全件、起動、大容量open、splitを今回の変更のために再試行しない。

証拠は`D:/NeNeNib/evidence/291-coalesce-improvement-20261004/coalesce-improvement-evidence.zip`。211ファイル・3750342 bytes、SHA-256 `615974ded12f7581f7fa08a8c6e1377191f08bbc0c95de7f62a69f703d2838e7`、CRC成功。細分診断/計装比較/通常比較の全rawと計画、24画像、IME画像、測定器、依存源、独立レビュー、失敗を含むビルド記録、3枝のgit bundleを収載。bundleの必要な親は`936b50b`で`git bundle verify`終了0。23:11の証拠束も入れ、以前の原記録を置き換えない。

判断: 重複削減の実装を作業枝へ保存し、PR #293はdraftのまま。空文書の性能受入・merge・split採用は保留。DPI物理遷移、device lost、描画中の実再入、InvalidateRect失敗は実機未再現。OS由来の断定も未了。適用は#291 / ADR 0011・0069 / ARC-001・004・007・011 / CPP-012・017 / QLT-001・012・013・014 / CNF-006 / GIT-004、Waivers: none。

### 保存済み単打の再解析（2026-10-05 00時台）

hideの約10分の調査依頼を受け、既存3実験の単打36本をrawから再解析した。製品commitは`dda40e4`のまま、追加のアプリ起動・計測・ビルド・製品変更は行っていない。対象は23:11の元版O対layout再利用A、23:37の入力後段細分版A、23:44の同じ計装によるA対重複無効化削減B。異なる実験の母集団は混ぜない。

| 確認する誤りと対象 | 実行したコマンド・結果・証拠 |
| --- | --- |
| rawの取り違え、単打/burstの混在、区間の欠落・重複・不正な集計 | `python -B D:/NeNeNib/scripts/291-single-decomposition-20261005.py`、終了0。source/raw hash、各202入力、単打がburst前に提示済み、各節目1回・順序、11隣接区間の和と元singleMsの一致を全36本で確認。handler補助分解は11区間へ重ねて加算しない |
| 解析実装に依存した見落とし | 読み取り専用の独立レビューで36 rawから区間、合計、variant中央値、各対応差を再計算し一致。COM参照の保持と解放位置もソース照合。`D:/NeNeNib/outputs/291-single-decomposition-20261005/review.txt` |
| 証拠束への原記録の取り違えと破損 | `python -B D:/NeNeNib/scripts/291-single-decomposition-archive-20261005.py`、終了0。収載元hash照合とZIP CRC成功。再測定ではない |

集計は`D:/NeNeNib/outputs/291-single-decomposition-20261005/decomposition.json`、解釈は同じ場所の`findings.txt`。解析スクリプトSHA-256は`45cce9ba2a9fde0966bb28281813c106354d8947c5eda7cddde49410890dacdd`。

| 同一実験内の単打区間 | 元版/候補の中央値 ms | 確認できた範囲 |
| --- | ---: | --- |
| 23:11 O→layout再利用Aの本文body | 0.2445→0.1185 | body単独の短縮を全描画の短縮とはしない |
| 同じO→Aのbody終了→EndDraw開始 | 0.2430→0.3030 | 対応A-Oは+0.069/+0.064/+0.014/+0.054/+0.064/+0.124ms、全6組で増加 |
| 同じO→Aの各試行のbodyと後段の合計 | 0.4875→0.4225 | 対応差は-0.042/-0.032/-0.178/-0.073/-0.062/-0.021ms、全6組で短縮。中央値同士を足した値ではない |
| 23:44 A→重複無効化削減Bの入力受信→handler戻り | 0.0370→0.0360 | この計装実験では中央値の増加なし |
| 同じA→Bのapply後handler / handler戻り→paint | 0.0160→0.0155 / 0.0260→0.0250 | flag追加が通常版の悪化と無関係だと証明したものではない |

23:44のB第4組は単打全体でAより+0.497ms。増分は本文後+0.207ms、本文前+0.076ms、本文+0.063ms、提示/戻り+0.063ms、EndDraw+0.058msなどに分布し、入力handlerの増分は+0.031ms。単一の区間だけへ原因を帰属しない。

ソース上、元版は`draw_plain_line` / `draw_composed_line`のローカルCOM参照を関数終了時にReleaseする。再利用版はlayoutを保持し、前フレームの未再利用layoutをステータス/palette描画後の`previous_body_layouts_.clear()`で破棄する。この寿命と位置の変化により費用がbodyから後段へ移る可能性がある。ただし最終参照解放、Direct2D内部の参照保持、実際の破棄費用は未観測。後段にはステータス/palette・clear・実行待ちが含まれるため「解放時間」とは呼ばない。bodyと後段の合計も短縮しており、改善が全部見かけの移動だったとも断定しない。

通常版O=`591b99f`対候補`a1fd1e1`で残る単打+0.1395msへ、別の計装実験の内訳を遡及して割り当てない。通常版の数値許容と安定条件の未達、OS原因未確定、WPR権限拒否は未解消。性能受入・merge・split採用はholdのまま。

次回案は同じ出力先の`next-diagnostic-plan.json`。**未実装・未ビルド・未実行**で、`draw_status_bar`直後と`previous_body_layouts_.clear()`直後の2節目を追加し、既存のbody/EndDraw境界と組み合わせる。D側の隔離worktreeで既存TimingPort/Win32 adapterと入力driverを使い、破棄順序は変えない。現在候補1版・固定12本・202入力・1280×800/DPI120・新規既定profileの区間診断を想定。新しい前面操作時間の調整と実行前のhash/出力先固定が必要で、通常版受入条件の代用にはしない。

恒久証拠は`D:/NeNeNib/evidence/291-single-decomposition-20261005/single-decomposition-evidence.zip`。45ファイル、497764 bytes、SHA-256 `ad6eb10b3d759d424439a73e1418383996a4a96aea12e7f6dbe196df0f02c8bd`。解析・所見・次回案・独立レビュー、3実験の元集計と36 raw、解析/梱包スクリプトを収載。CRC成功は同じ場所の`archive.json`に記録済み。既存の原記録・証拠を置き換えていない。

今回の日報・引き継ぎ保存は文書と参照先だけを変更し、関連する製品・テスト・依存は不変。`git diff --check`は終了0、新しい2文書のローカルリンクと見出し参照6件、および日報のIssue要約6行を`python -B -`の限定照合で確認した。先行するRelease/静的検査/表示/IMEの成功結果と性能holdをそのまま再利用し、アプリの再起動や全件検証は行わない。適用は#291 / ADR 0011・0069 / QLT-001・012・013・014 / GIT-003・004。製品schemaと受入幅の変更なし、Waivers: none。
