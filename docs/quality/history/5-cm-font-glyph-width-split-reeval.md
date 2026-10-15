# 5-cm — 字体選択・可視字形・表示幅とsplit再評価（Issue #291・ADR 0071・0073・0074）

出典: docs/quality/gate-proofs.md の節 5-cm（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-05夜、hideの「思いつく対応を全てやってみて」を受け、長行の残費用を診断して複数案を隔離実装した。
通常候補は大幅に短縮したが、**長行単打の前後半安定条件とsplitの総時間2ms条件が未達なので、性能の総合受理・main統合・split採用は保留**。閾値・性能基準値・過去の失敗記録は変更していない。
集計と各版の全6値、判定理由、実行前plan、exeの生成元/hashは[`render-capsule-2026-10-05.json`](../render-capsule-2026-10-05.json)。

### 実装と試した案

正典は既存の `display_width` → `DisplayLine` → `layout_of` → DirectWriteの経路。
`FontFallbackCache/Key/Entry/Request` は完結した最大256 UTF-16単位のOS字体選択を128件まで保持し、範囲外・未知のsource・OS失敗は同じOSへ委譲する。
`BodyGlyphCollector/Run` はDirectWriteの所有済み字形・位置を保持し、保守的な上界で画面外と証明できるrunの描画だけを省く。未知の装飾は収集全体を棄却して既存DrawTextLayoutを使う。0字形のTabは描画APIを呼ばない。
`VirtualColumn.cpp` は唯一の `DisplayWidthRange.hpp` から65536 byteのBMP索引をコンパイル時生成し、BMP外は従来の検索を使う。文字幅の意味の表を増やしていない。
`Direct2DRenderer` / `BodyTextLayout` とCMakeの所有先へ接続し、`DisplayLineTests.cpp` に全コードポイントの正本照合を追加した。公開のsplit機能・保存schema・別の状態所有者は追加していない。

| 案 | 観測と判断 |
| --- | --- |
| OS fallbackの明示設定、font axis/optical size、locale指定 | 独立DirectWrite診断では約16〜19msの文字組みが残り、採用しない |
| 観測した字体を全区間へ先に指定・runをまとめる | 約0.56msへ短縮するが、遠端の位置/HitTestに最大0.16015625px差。文字位置を変えるため不採用。collectionを省く全量MapCharactersはASCIIの字体も変わり、同じcollectionを渡す版は約142ms。いずれも不採用 |
| 完結したOS字体選択要求の保持 | 診断で字形・位置・全UTF-16位置のHitTestが一致。通常Releaseの長行単打28.5385→16.5005ms（各6本）。採用する通常候補に含める |
| 透明な行bitmap | Consolasの3場面で2〜18画素にRGB各1階調差。不採用 |
| 背景と文字を合成済みの不透明bitmap | 22場面一致、通常単打28.461→7.9965ms。ただし200文字の1本は292.590ms（入力到着範囲285.567ms）で保持。選択/検索で通常描画へ戻る条件とbitmap資源を増やすため、最終候補は字形保持へ一本化。実験枝のADR 0072と全記録を保存 |
| 可視字形の保持 | Tabの0字形でE_INVALIDARGになった初版を修正し、22場面一致。通常単打28.5585→7.6505ms。候補単打の前後半差1.677msは許容0.76505ms外で、成功に読み替えない |
| 正本の幅表からBMP索引を生成 | 全1114112コードポイント一致。volatile入力の診断で同じ結果を45.42〜46.08→7.46〜7.56msで求めた。最初の定数ループはコンパイラが外へ移せるため速度根拠から除外し、記録を残した。最終通常候補に含める |

各行の通常比較は別時刻の固定6組で、bitmapと字形保持の数値差を直接の優劣の証明にしない。安全な全文字組みの範囲を壊す先頭切り捨て、欠字になるfallback無効化は採用しない。

### 変更範囲の検証

| コマンド・対象 | 回帰の理由と実結果 |
| --- | --- |
| 各候補で `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` | SDKのCOM境界、CMake追加、実際の最適化構成を確認。字体保持の初回54403dfは固定11引数のtidy違反で失敗し、4行の境界へ閉じWVR-0001適用後の382800fは成功。字形9900507、幅c637acc、最終311e78b、整形後f42a95d、S/H/VのReleaseは成功。全ビルドログを保存 |
| 実ソース `FontFallbackCache.cpp` / `FontFallbackKey.cpp` を `clang-cl /std:c++latest /Od /MT /W4 /WX -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all` でContractsStrict.cppと実行 | 誤一致・借用寿命・失敗の記憶・上限越えを防ぐ。604 checks成功。全文/属性/COM identity、範囲/名前上限、prefix/tail、QIのE_FAIL、OSのE_FAIL/S_FALSE、null font/scale、128件FIFOを確認。独立診断exeであり製品Releaseではない |
| 実ソースBodyGlyphCollectorを同sanitizerでGlyphPixels.cppと実行 | 収集後の参照寿命、clipによる欠け、代替描画。3字体×3サイズ×4幅×5文字列の180画素対、1131保持runが全て0差。layout/format/collectorを破棄後に所有字形を描いた。3未対応コールバックの拒否も成功 |
| `cmake -S . -B build/debug-width -G Ninja -DCMAKE_BUILD_TYPE=Debug` / `cmake --build build/debug-width --target nib_tests`、`build/debug-width/nib_tests.exe --display-line` / `--vim-virtual-column` | 幅の索引が表示文字列と仮想桁の意味を変えないこと。Debug ASan/UBSan、display-line 1114301 checks、vim-virtual-column 424 checks成功。他scopeは実行しない |
| `291-font-fallback-visual-20261005.py` と `291-visible-glyph-fixed-visual-20261005.py` の前後比較 | 編集・クリック・選択・検索・scroll・幅・3書式・テーマ・多言語/結合/Tab/双方向の表示を確認。各22場面で本文/ステータス0画素差。比較元22画像は同じ0dfad49の成功結果を再利用。初版71a2577の起動失敗も記録し、Tab修正後だけ再実行 |
| `python -B D:/NeNeNib/scripts/291-render-final-ime-20261005.py` | composed行の新描画経路とIME状態復元。311e78bで実キーによる日本語入力・Space変換・Enter確定、対象文節、ink/下線522/189→確定651/0、通常/Vim切替とIME開閉復元が成功。verify_imeだけを呼び、関係ないGUI試験は実行しない |
| 変更ソースの `git-clang-format`、既存conformanceのsource/document/waiver/architecture限定呼出し | 整形と新規型/翻訳単位の所有、規則の遵守。整形後0差、0 findings。誤ったPython module登録で検査補助が1回失敗し、sys.modulesへの登録を直したconformance2で成功。製品不具合と混同しない |

独立sanitizer診断2種は起動時に `interception_win: unhandled instruction` の警告を出した。終了0で検査は成功しsanitizer違反の報告はなかったが、全OS呼出しの完全な計装を主張しない。ログをそのまま保存した。

通常実測は **311e78b**、SHA-256 `5F2CFF9C74C37FC4BBE2D43E7481AC1CABBBDA7ADA8744B5E30EE37504AECB66`、1399296 bytes。
製品ソースの保存版 **f42a95d** はその後の空白整形とown-headerのinclude順だけで、対象差分を機械照合した。Releaseを再ビルドして成功、SHA-256 `44B017F33241E320BC5F182CEEB1CAFC6899255F898C0AC943703BC04F023900`、同bytes。動作・試験・依存が変わらないので、画素/IME/性能を再実行しない（QLT-001 / QLT-012）。

### 最終通常版とsplitの固定90試行

`python -B D:/NeNeNib/scripts/291-capsule-combined-20261005.py --output D:/NeNeNib/outputs/291-capsule-combined-20261005 --manifest D:/NeNeNib/briefs/291-capsule-combined-manifest-20261005.json --prepare` / 同outputの `--run` は終了0。
3本文×6組×O/A/S/H/V、奇数O→A→S→H→V・偶数逆順。既存 `keys_trial` / driver、1280×800 / DPI120 / Cascadia Code 13.5pt、新規profileを実行前固定。
Oは0dfad49、Aは311e78b、Sはdad9ab0、Hはc1ef96c、Vは9bb7d68。全90本が有効、各202入力、時刻単調・暖機と単打の独立・最終提示・記録上限4096未満・幾何・hashを確認。欠測/除外/再試行0。解析器は正例と5負例を確認。
空文書burstの到着範囲50ms条件を維持。長文書の到着範囲は記録し、空文書の条件を転用しない。

| 本文・指標 | O 中央値 / 最大 ms | A 中央値 / 最大 ms |
| --- | ---: | ---: |
| 空文書・単打 | 0.7395 / 1.285 | 0.7355 / 0.772 |
| 空文書・200文字 | 3.2275 / 3.879 | 3.1135 / 3.339 |
| 16 MiB短行・単打 | 1.2650 / 1.298 | 1.2430 / 1.359 |
| 16 MiB短行・200文字 | 8.6080 / 10.179 | 7.1165 / 7.289 |
| 長い日本語行・単打 | 28.5340 / 32.491 | 7.2695 / 8.107 |
| 長い日本語行・200文字 | 175.1165 / 181.212 | 91.5315 / 92.739 |

長行単打の中央値は74.5%、200文字は47.7%短縮し、両指標とも対応6組全て短縮した。空文書/短行の通常比較は従来条件内。
長行A単打は前半/後半中央値差0.800msが許容0.72695msを超える。速度改善は確認できるが、**総合holdを維持**する。OS等への原因帰属はしていない。

| 本文 | A 通常 | S 1枠試作 | H 上下 | V 左右 |
| --- | ---: | ---: | ---: | ---: |
| 空文書・単打 ms | 0.7355 | 0.7605 | 0.7440 | 0.7435 |
| 16 MiB短行・単打 ms | 1.2430 | 1.2270 | 1.2300 | 1.6610 |
| 長い日本語行・単打 ms | 7.2695 | 7.1070 | 6.8035 | 6.4095 |
| 空文書・200文字 ms | 3.1135 | 3.1715 | 3.0705 | 3.2455 |
| 16 MiB短行・200文字 ms | 7.1165 | 7.2495 | 7.2005 | 7.4520 |
| 長い日本語行・200文字 ms | 91.5315 | 90.3875 | 89.6230 | 91.3815 |

空文書と16 MiBはSの許容/一貫悪化条件、H/Vの増分と2ms条件内。長行はA安定条件に加えH/V総時間が2ms外なのでsplit不採用。V単打自身にも前後半差0.910ms > 許容0.64095msがある。
各本文のO/A/S初期画面は本文/ステータス0画素差、H/Vの配置も保存画像で確認。Hは合計表示行数が19→18となり、Vは同じ行を同じ幅で共有するため、Aより小さな観測値を「二つの独立文書でも軽い」と解釈しない。独立文書・scroll・2つ目のframe生成・所有移行は未測。

### 長行単打の分布

`python -B D:/NeNeNib/scripts/291-render-final-tail-20261005.py --prepare` / `--run` / `--analyze` は終了0。固定6組、各起動暖機1+40文字、名目100ms間隔で各版240件。長行fixtureへ追記し、通常の202入力試行とは別の実験。解析器5負例と、全41入力が次入力前に提示されたことを確認。全480件有効、除外・再試行なし。
runnerの継承によるtrial名の `empty-*` は識別ラベルだけで、実際の起動引数・fixture hash・planのstimulusは長い日本語文書で固定されている。

| 指標 | O ms | A ms |
| --- | ---: | ---: |
| 中央値 | 32.4595 | 6.8750 |
| p95（nearest-rank） | 36.362 | 7.513 |
| p99 | 37.198 | 7.652 |
| 観測最大 | 37.644 | 7.978 |
| 16.7ms超 | 240/240 | 0/240 |

2ms/5ms超は両版240/240。しきい値は説明用で、受理条件を変更しない。入力受信からPresentの戻りまでをC++のQPCで測り、Pythonは刺激と集計だけ。OSの入力queue滞留・物理的な発光時間を含まず、240件と起動内の相関から恒常的な最悪時間を保証しない。

### 保存と残る範囲

`python -B D:/NeNeNib/scripts/291-render-capsule-archive-20261005.py` は終了0。
`D:/NeNeNib/evidence/291-render-capsule-20261005/render-capsule-evidence.zip` は1023原ファイル+manifest、46224978 bytes、SHA-256 `4ea93d57c3a7ddf6d9a8ca01bd5f7d71a033f05607a18bef7fb88648c07c1bad`。
全entryのSHA、ZIP CRC、git bundle verify成功（親936b50bが必要）。失敗した起動・bitmap画素差・遅い試行・定数ループ診断・全候補のexe/metadata・scripts・fixtures・raw・profiles・画像・9枝を含む。後続の集計と文書検査は同evidenceの補足へ保存する。
`291-render-capsule-record-20261005.py` で保存90本のschedule、raw hash、全指標を再計算し一致した。測定の追加はしていない。

初回/編集行の全文字組み、長行ごとのDisplayLine再生成と入力ごとのframe生成は残る。字体の区切りを変えずに全文字組みを部分化する設計、frameの重複生成を省く設計は追加の所有/IME契約を要し、今回完了とはしない。物理DPI遷移・device lost・確保失敗の実再現も未了。
新規8 worktreeと従来の比較元は未受理・main未統合で、測定済みexeへの参照もあるためD側に保持する。具体的なパスは引き継ぎと証拠のworktree-inventory.jsonに記載。技術受理・統合・必要反映が完了したら安全確認後に整理する。
適用: #291 / ADR 0068・0071・0073・0074 / ARC-001・007・011 / CPP-012・016・017 / QLT-001・004・012・013・014 / GIT-003・004。
保存schema変更なし。Waivers: **WVR-0001 / WVR-0002**（SDK固定COM署名の引数数のみ、2026-11-04期限）。ゲート閾値・抑制範囲を広げていない。

追加の `python -B eng/protected-diff.py --base d620e1a --head f42a95d` は終了0。fixture 1853→1853、metadata/deleted/changed/added=0、保護ファイル変更なし。scope33の比較件数は未測で、件数不変とは主張しない。文書保存では `291-render-capsule-document-check-20261005.py` が終了0、document/waiver 0 findings、追加ローカル参照15件、plan/raw/tail/ZIP hash一致、製品差分なしとwhitespaceを確認した。全件テストは実行していない。
