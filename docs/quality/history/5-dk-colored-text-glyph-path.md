# 5-dk — 着色文字も既存の字形経路で描く（Issue #362・ADR 0098）

出典: docs/quality/gate-proofs.md の節 5-dk（2026-10-16 に D50 で移動・本文は移動時のまま）

**技術受理**。R3の残りを専用条件で比較し、本文の混在長行に大きな利益を確認した。製品の変更は `Direct2DRenderer.cpp` の `tint_runs` 内の一呼出しを既存 `draw_body_text(text, area, NONE)` へ置換し、不要originを削除する二編集のみ。元からあるbrush、HitTest範囲/順、clip push/pop、paint順、保持/失効、未登録または!glyphs_readyのDrawTextLayout fallbackを保つ。取得成功の空glyph列は正常な空描画でありfallback条件とはしない。一覧入力欄は従来どおり本文保持に未登録で、正規fallbackを通る。

先行ADR `30394c9`、製品 `36ebb6154935ad1722f7007b3e6a64e872424aa6`、文書訂正 `9218124` / `3b00a4d`。通常Releaseは9218124、Debugは36ebb615の同一製品source。基点main `3b114d6` と前版ca4ad39の製品/test/eng/flagsは一致。測定時HEAD `3b00a4d4a06bc935dc07a2f1d65f162fd7caf123` はclean。実装build前後は524source指紋と各参照object/実flagsを照合し、Renderer全文への厳密二編集と他の全関数不変を確認した。測定前後auditは実装source1・道具15・3exeの19path、HEADclean、identityを照合した。identity SHA256 `9cc9a4430351750c76f7ada82ee5ff7dcd83c82d2f300e3da831420c83cfd192`。

規則: ARC-001/002/004/007/008/011、CPP-003/008/011/012/016、QLT-001/002/004/008/012/013/014、CNF-001/002/003/004/009/012、D41。waivers **none**。公開API/source/target/独自描画器/decoder/cacheを追加せず、schema・fixture・警告・抑制・基準・許容を変更しない。

### 固定した製品応答比較

i9-10850K / RTX 3090、電源HP推奨（GUID48684d4a-8524-4093-8a63-ea7132b79c1c）、window DPI120、同じ絶対文書path/設定/system/Cascadia Code13.5/guide on/1280×800。通常Release前後の `--measure` と正規window_driverを使用。ASCII `a\x01 `×1024、混在 `日a🖋\u200b `×1024、短行 `a\x01 `×2、すべて末尾CRLF。検索 `/a<CR>` 後に最初のaへgg0/gg0l/gg0で固定し、暖機nNの後、交互nN20入力をABBA×3。一条件120対応組、全36process/720計測input。間隔はASCII/短行0.4秒、混在3秒。起動/準備/検索生成は区間外、input_receivedから次input前の最初のframe_presentedまで。Present復帰でありGPU完了やtint単体時間ではない。

| 条件 | 前版中央値µs | 後版中央値µs | 対応比中央値 | 対応比min–max | 短縮/同/遅延（120組） |
| --- | ---: | ---: | ---: | --- | --- |
| replaced-ascii-1024 | 64760 | 64106 | 1.001570153658 | 0.899039897589–1.118709999169 | 56/0/64 |
| replaced-mixed-1024 | 1.79317e+06 | 20619.5 | 0.011477568240 | 0.010769417527–0.014784208061 | 120/0/0 |
| replaced-short-2 | 601 | 605.5 | 1.013963963964 | 0.620689655172–1.792968750000 | 51/1/68 |

混在は120組全て短縮、対応比中央値約0.01148を確認したため採用する。ASCIIは対応比約1.00157で横ばい。短行は601→605.5µs（4.5µs増）、対応比約1.01396で68組遅く、無退行や全入力高速化とは記さない。中央値同士の比と対応比中央値は別値である。全36trialは初回exit0、各trialの暖機可視変化とn/N後復帰0差、case内の99画像比較も0差。独立validatorは暖機72と測定720の全792入力に次input前frameがあり、計測値と原区間が一致することを確認した。測定前後のCPU/GPU/powerPlanは同一、他のbuild/test/GUI/重処理は停止。計測中の作業は小さな読取/記録と未実装の次案調査だけ。

### 直接境界と実行記録

以下の`<WT>`は測定時 `D:/NeNeNib/worktrees/362-tint-glyphs`、`<OUT>`は `D:/NeNeNib/outputs/20261010-tint-glyphs`。整理後は後述の恒久先に同じ相対配置で原記録を保持する。成功済み結果を工程・担当・文書commitだけの理由で再実行しない。

| 対象と退行リスク | 実行コマンド/記録 | 結果 |
| --- | --- | --- |
| 固定tool/外部flag、単一sourceのformatと規約 | `eng/toolchain.ps1`、`clang-format --dry-run --Werror --style=file:<WT>/.clang-format <WT>/src/ui/win32/Direct2DRenderer.cpp`、`python -X utf8 <WT>/out/362/focused-conformance.py` | 初回exit0、0違反。waiver none |
| 呼出し型/警告/tidy/実製品sanitizer | `cmake -S <WT> -B <WT>/build -G Ninja -DCMAKE_BUILD_TYPE=Debug`、`cmake --build <WT>/build --target NeNeNib -j 2` | 初回116段成功、正規C++23/clang-cl19.1.5/ASan/UBSan/no-recover/tidy。NeNeNib targetと必要依存のみ |
| 二編集限定と他source/試験/flags不変 | `python -X utf8 <WT>/out/362/source-evidence.py`、`finalize.py`、`git diff --check` | 初回exit0。implementation/report.md/manifest.json/rawに全command/実flags/指紋 |
| 通常Release条件 | `pwsh -NoProfile -File eng/build-release.ps1 -Ref 9218124` | 初回116段/139.631秒、exit0。out/release/9218124.json |
| 実Rendererの色・clip・字体・重なり | `python <OUT>/verify-362-tints.py --label before --executable <前版>`、`run-gui.py --label debug/after --executable <対象版>`、各scriptの`--compare` | 3style×9scene=27の前後本文/status完全0画素差。18刺激遷移は全て可視変化あり。Debug実起動/正常終了 |
| IMEの本文保持と入力欄fallback | `verify-362-ime-body.py`、`verify-362-ime-inputs.py`をbefore、同run-gui.pyでdebug/after、各`--compare` | 本文4＋palette6場面も0画素差。実SendInputのother色0→204→0と0→83→0、本文Space accent285。0x411、open status0へ復元。合成IMEなし |
| 道具の入力対応と短行/長行の可視変化 | `compare-362.py --smoke before/debug/after --before <対象版>`、`validate-362-marks.py <成功3trialのfolder列> --expect-trials 3 --output <warmup.json>` | 全3caseずつ成功。旧版ASCII/短行v2成功を再利用し、混在だけ修正版v3を別folderで確認。速度採否には使わない |
| 固定比較と全入力の独立監査 | `compare-362.py --before <ca4ad39 Release> --after <9218124 Release>`、`validate-362-marks.py <OUT>/comparison --expect-trials 36 --output <OUT>/warmup-comparison.json` | exit0、36trial/360対応組/720計測/72暖機、99画像比較0差 |
| 同じ製品/入力/環境の証明 | `prepare-measurements.py`、`environment.ps1 -Label before/after`、`post-measurement-audit.py` | exit0。19pathのSHAとHEADclean、正式再利用原記録の不変、前後snapshotで重処理なし（測定中は作業lock）。JSONに正しいHP推奨を保持 |
| 独立レビュー | review/design.md、product.md、harness.md、acceptance.md | 厳密二編集、other節到達、全試料/暖機、正式再利用、短行の増加を含め確認。阻害所見は採用前に解消 |

GUIはASCII制御文字/BMP/補助平面/結合/Tab/双方向/複数置換、bar/normal/検索/選択/line/block、狭幅右clip、異なる字体/size/themeを含む。本文IMEは差込前後に置換文字、一覧IMEは160字のprefixによる左への横溢れを含む。全対象processは正常終了、文書/設定不変、本文IMEのdirtyは確認画面でdiscard。親は画像も読んで到達を確認した。

### 再利用・失敗保持・限界

正式single/burst200/burst200-16MiB/single-long-lineは#360の成功を再利用し、#362で再実行したとはしない。これらは制御置換のないASCII/日本語/空文書でIMEも作らず、変更tintへ非到達。draw_body_text/collector/幾何/他Renderer関数/core/application/tests/eng/flags/基準は不変。保存済み前版exe、正式record、script/referenceのSHAを照合し独立レビューで経路を確認した。既存WICのNONE+caller clip契約もdraw/collector/testが不変なので再利用し、実Renderer到達は新しい37場面で別途確認。全scope/全unit/全速度8条件を新実行しない。

初回smokeは混在のframeが0.4秒を超えて入力が合流、短行は検索開始点からの折返し通知で復帰不一致。v2の混在gg0は日を指しており最初のaではなかった。初回/v2 script/plan/rawを残し、間隔3秒・混在gg0l・準備input7/8/7を比較開始前に固定した。ASCII/短行v2の成功を再利用。速度試料の選別や合格までの再測定はない。継承planの旧source/GUI metadataも元を保存して訂正した。

初回IME道具は検索欄がIMEを受けると誤認したが、既存ImeStance::closedとcomposition_ignoredでother色0の未到達となった。原script/rawを残し、実到達する本文/paletteへADRと検証範囲を訂正した。これは製品退行ではない。ADR名/ログpath/メタデータpathの推測読取失敗、無いprocess名の監視、出力truncate/端末文字化けもjournal/rawへ記録。製品build/採用用比較の失敗を隠したものではない。

実Rendererの資源失敗注入、RSS/実確保数、任意装飾・全フォント・DPI・IME・全入力の同値、全般的な描画速度は未測。画面外clip省略やCPU samplerは次案の読取調査だけで、製品変更/実行/速度効果を主張しない。

### 恒久収載と後続

全source/通常DebugとRelease/実flags/FileAPI/GUIとIME画像/原marks/全試料/初回道具失敗/独立reviewを `D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-362` へ元先SHA照合して収載した。740files/27924676bytes、manifest SHA `125de7a682328a341a24698d135927314a1df088645a64f7fe71d7179d31c3f1`。前版はsnapshot-360、後版はsnapshot-362/files/worktrees/362-tint-glyphs/build/release-9218124/NeNeNib.exe（SHA `ce0277729b358d2d66f2dda65d592b62e3a159eecb7dbe3e9b85a744d0a88f77`、1449984bytes）、DebugはSHA `791642571062a2af35c1c4cce2fb1264a77812961c0c3af3743fb7225365255c`、17022976bytes。CI/main/追加文書とcleanupはacceptance-362へ後続保存。統合・唯一成果・未保存/未追跡/ignored・稼働参照・links・絶対D pathを監査して追加worktree/不要出力を削除し、branch/commitは保持する。
