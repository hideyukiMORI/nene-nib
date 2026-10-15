# 5-eb — 最新実装で旧split描画試験を再評価（Issue #404）

出典: docs/quality/gate-proofs.md の節 5-eb（2026-10-16 に D50 で移動・本文は移動時のまま）

Issue / 規則 ID: #404、D40/D41、ADR0068/0075、ARC-001/003/004/007/011/012、CPP-002/008/012/016/017、QLT-001/002/012/013/014、GIT-001〜004。
変更したファイルと振る舞い: 通常PRは準備資料・ADR0068の追記・本節・実測JSON・日報・引き継ぎ・currentの7文書のみ。製品mainのsrc/API/settings/schema/flags/基準値は変更しない。隔離したS/H/Vのrenderer試作は実験branch/commitと恒久証拠にだけ保持する。
実行した検証コマンドと結果: 下表。72試行を一度だけ実行し、原raw監査・初組12PNG目視・画素比較は成立。旧数値条件の総合判定はholdであり、正式QLT-014の合格や製品split採用を意味しない。
ドキュメント・スキーマの変更: 既存計画へ現状確認の結果を追記。新規実測JSONは記録成果物で、製品schema/規則/閾値/実装経路は変えない。
Waivers: none。
残るリスク: 同じframeに有利な再利用、Hの表示行数減少、別文書/独立scroll/二つ目frame/所有移行/undo/save/IME/hit test/session未実装、入力前だけの画素確認、案内の未分割、別日比較の帰属限界。判定の揺れと2ms未到達を下記へ保持する。

### 再評価の来歴と必要な検証

hideは17:48 JSTに「splitの時と同じ試験」を確認し、17:49 JSTに「やってみて」と依頼した。継続改善の自動再開ではなく、この72試行の現状確認だけを実施した。単体実行、独立agentの追加0。17:55の計画commit `f37b83a` より後に試作・build・測定を行った。基準main `4f8db90e7a44fa7a34aec81a86461dc6e4f1418c`、通常Aの製品commit `cf27a3ad2aacc0ffd3384fa8cee7a6522c80b41f`。

S/H/Vは旧#290の`body_partition`・clip付き`draw_body`・`draw_partitioned_body`をそのまま移し、同じframeを分割矩形へ渡す。S/H/V相互は分割定数だけ。Aから変える製品ファイルはDirect2DRenderer.cpp/.hppの2つのみ、案内など現行の別処理は保持。旧driverは全文一致、旧measure-speedの既存関数は測定入口2個を除いてAST一致、既存定数はBENCHES以外一致。追加された正式long-lineベンチの入口は今回通さず、旧`keys_trial`を一度ずつ直接呼ぶ。

| 版 | source commit | exe SHA-256 | bytes |
| --- | --- | --- | ---: |
| A | cf27a3ad2aacc0ffd3384fa8cee7a6522c80b41f | 3dc8a0a8feec808609b30d6d1feea870801bb093cc507628ac12f2b9025a2b5c | 1444864 |
| S | a165a25b4af328d626724a4f3f2b4adf672db1ef | 4cb90abc1f6edd34b0fa303140bd84e5d5e30ac062b73dbfab29377a38305e4b | 1444864 |
| H | 887c04f0177f43e065f6cf81ddfbad14d320a533 | de96f0853478e5ad2c0655ada8f2d8b7930f55cf9b2ad598ea6a34954af6516f | 1445376 |
| V | 2fa7e4fc307af4c880567477350ab19df85f6ae6 | a52a3897d128de4e31a02f3d418ce3a9a37989ecda2e6c48f8881057a7511662 | 1445376 |

Aはsnapshot-402の正規Releaseを再利用。cf27a3a→main4f8のsrc/eng/tests/CMake/.clang設定は同一。全4構成の200 compile commandsはsource/build pathと未buildのnib_perf_probesのcommit metadataだけを正規化すると一致し、製品のflags差はない。S/H/Vは40hex Refとclean HEADから正規build、build/test/hash/copyを全て終えてから窓を排他で測った。

OUTは`D:/NeNeNib/outputs/20261011-split-recheck`。全nativeの開始/終了/exit/stdout/stderrは`command-capture/<label>/`。恒久配置は`D:/NeNeNib/evidence/split-recheck-20261011/snapshot-404/files/outputs/20261011-split-recheck`、統合・収載・整理の実完了は`D:/NeNeNib/evidence/split-recheck-20261011/acceptance-404/closeout.md`に記録する。rawの旧絶対パスは収載時のpath mappingで追跡する。

| 対象・退行リスク | 実command / capture label | 結果・証拠 |
| --- | --- | --- |
| 試作の構文・警告・静的解析・正式なRelease条件 | 各WTで`pwsh -NoProfile -File eng/build-release.ps1 -Ref <上表40hex>` / build-s,h,v | 各exit0、117/117、約306秒。toolchain clang-cl/tidy19.1.5、Release、Ninja、CMAKE_BUILD_PARALLEL_LEVEL=2。3 native session 37376/33036/53015完了後に測定 |
| 変更2ファイルの整形 | toolchain環境で`clang-format --dry-run --Werror src/ui/win32/Direct2DRenderer.cpp src/ui/win32/Direct2DRenderer.hpp` / format-s-toolchain,h,v | 全3 exit0 |
| 欠測・時刻・幾何・旧判定の補助道具 | `python -B -X utf8 audit.py --self-check` / helper-check | exit0、正例/反例15。GUI起動なし |
| 比較元・旧試作・刺激・コンパイル条件の混入 | `python -B -X utf8 prepare.py` / prepare-metadata | exit0。原差分・200 commands・fixtures・source/exe SHAと固定planを保存 |
| 旧条件の入力からPresentまでの費用 | `python -B -X utf8 run.py` / fixed-run | 18:03:34〜18:11:53 JST、native session92409 exit0、72/72有効。系列完了のexitであり条件合格ではない |
| 集計誤り・欠落・外乱の誤受理 | `python -B -X utf8 audit.py` / raw-audit | exit0、72rawの14,544input/15,624marks、暖機/単打の独立・最後のframe・全数値を別の計算で照合。guard5項目すべてtrue |
| 描画分割の欠落・本文外の差・元fixture破損 | 原PNG12枚をoriginalで目視、`python -B -X utf8 visual-audit.py` / visual-audit | exit0、A/S3対0px、H/V6対は本文だけ、mask0。fixture bytes/SHA/mtime不変 |

この後の7文書の結果保存にアプリのunit/oracle/全件回帰/正式8指標を追加しない。Aの製品・測定器・入力が不変である根拠はplanのreuseProofへ保存し、#402の対象unit/oracleを工程のために繰り返さない。試作の描画変更自体は上記Release・72試行・初組画面で確認した。新しい正式ゲートの測定・製品splitの全機能検証とは名付けない。

### 固定入力と全試行

i9-10850K / RTX3090 / DPI120、指紋bc8a356f37c68491、HP推奨power GUID48684d4a-8524-4093-8a63-ea7132b79c1c。LastInput前後30592843で不変。窓/クライアント1280×800、既定Cascadia Code13.5pt、毎回新しい私有profileでsettings.v1/v2なし。日本語fallbackの実字体は未測。入力前とclose前の前面/5点非被覆/幾何/DPIを記録した。

空文書、旧`large.txt`16,800,000 bytes、旧`long-japanese.txt`6,219,000 bytes（SHA ef2d1511a696984dcd4184498a852d944a8ca366bf847f440d1e46b73b32a6c5）。各本文6組、奇数A/S/H/V・偶数V/H/S/A。暖機w1字、単打a1字、b200字。空文書の到着幅<=50ms、ファイル本文の到着幅は説明値。除外・外れ値削除・穴埋め・系列再試行0。全6値/min/max/対応差/正差数/前後半の値は[実測JSON](../split-render-2026-10-11.json)。6試料からp95は出さない。

計画SHA `d459d8a78378f53a05d3e8c1e4bea1ee3dd0566bdc495b066eff9e506f3c9048`、raw監査SHA `cd25c694ba4daaf9410a39db676c241620400e95905d7c3a201e603e1f4912fa`。実測中央値（ms）:

| 本文 / 指標 | A 通常 | S 1枠試作 | H 上下 | V 左右 |
| --- | ---: | ---: | ---: | ---: |
| 空文書 / 1文字 | 0.5950 | 0.6820 | 0.6980 | 0.6915 |
| 空文書 / 200文字 | 2.0670 | 2.0485 | 1.9255 | 1.8280 |
| 16 MiB短行 / 1文字 | 0.8745 | 0.8765 | 0.8645 | 1.1780 |
| 16 MiB短行 / 200文字 | 2.2920 | 2.3845 | 2.3165 | 2.3145 |
| 長い日本語行 / 1文字 | 6.7285 | 6.3780 | 6.2940 | 6.3565 |
| 長い日本語行 / 200文字 | 7.1070 | 7.2480 | 6.8575 | 7.3815 |

### 旧条件による判定と別日比較

- 空文書: A単打の前/後半0.530/0.687ms、差0.157ms>許容0.10msで本文全体hold。S単打は増分0.087msで数値範囲内でも正差5/6でhold。
- 16 MiB: A単打の差0.001ms<=0.10ms、連続入力の差0.070ms<=0.50msで安定。H/Vは単打増分−0.0100/+0.3035ms、合計0.8645/1.1780ms、連続増分+0.0245/+0.0225msで数値条件内。S連続入力は増分0.0925msで数値範囲内でも正差5/6によりhold。
- 長行: A単打の差0.065ms<=0.67285msは安定だが、A連続入力の前/後半7.986/6.782ms、差1.204ms>許容0.7107msで本文全体hold。H/V単打はAへの増分−0.4345/−0.3720msでも合計6.2940/6.3565ms>2msで数値条件外。連続の増分−0.2495/+0.2745msは1ms以内。

全体を採用可へ読み替えず、ADR0068/0075の保留を維持する。幾何・入力の監査成功はノイズの無さを意味しない。事前の揺れ条件はそのまま残し、原因を推測で特定しない。別日10月5日と比べた長行Hの単打は6.8035→6.2940ms（約7.49%短い観測値）、Vは6.4095→6.3565ms（約0.83%）。連続入力はH89.6230→6.8575ms（約92.35%）、V91.3815→7.3815ms（約91.92%）。当日A/Bや個別改善の因果率ではなく、10月5日以後の全変更を含む参考比較である。長行の総2msまでなお約68%の短縮を要する。

### 画面・準備失敗・完了境界

初組の12PNGをすべて原寸で目視。A/S全3対0px、H/Vは本文内だけが変化し、タイトル/通常モード/行1桁1/UTF-8/CRLFは同じ。Hは9行×2、A/Sは19行、Vは19行×2を表示し、同じframeを再利用。左右の行末clipは予定の幅の差。空文書Hでは既存の全体配置の案内と下枠先頭行が重なる。製品見た目の受理ではなく、暖機1字で消える案内の枠対応は未実装として残す。撮影は入力前だけで、打鍵後の全画素を検証したとはしない。

準備時のbare clang-formatはPATH外のWinError2でnative未起動。`format-s`の未完record/空streamsとsession-journalへtool tracebackの要点を残し、既定toolchain環境で成功した。最初のprepareは全200 commandsの比較で未buildのnib_perf_probes用NIB_PROBE_COMMIT差を検出してexit1。原script `prepare-initial.py` とrawを保持し、source/build pathに加えてその正確なcommit metadataだけを正規化してprepare-metadataで一致を確認した。compiler flags・測定刺激・試行・閾値は変更していない。いずれも最初の窓起動より前で、測定の再試行ではない。推測ADR pathのread失敗は実在pathで読み直し、測定へ影響しない。

7文書の差分・JSONとの一致・リンク/規則を確認し、通常draft PR/Ready/必須CI/`eng/merge-pr.py`同head plan→execute/main同期へ進む。試作sourceをPRへ入れない。恒久収載では原ログ・全72試行・12PNG・script/plan・source refs・Release exe・CMake/Ninja/FileAPI/compile commands・失敗原版を保存する。実完了は上記acceptance-404/closeout.md。追加D worktrees/OUTは絶対path・dirty/untracked/ignored・唯一成果・稼働参照/linkとarchive SHAを確認して整理し、実験branch/commitを保持する。完了後は停止し、継続改善を自動再開しない。
