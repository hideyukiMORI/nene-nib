# 5-dl — 完全画面外の着色clip省略は実験不採用（Issue #364・ADR0099）

出典: docs/quality/gate-proofs.md の節 5-dl（2026-10-16 に D50 で移動・本文は移動時のまま）

設計固定は2509493、実験製品は0c91c990446e70e90c62e05b0894d5ccbfaa206d、測定HEADは5bf99af17baa01370f4de86f7a5baee6a6692c91。tint_runs内でtarget幅widthを一度取得し、`run.left > width || run.left + run.width < 0` のrunを省く二挿入を試した。今回の固定全画素条件を満たさず、ASCII固定比較も欠測となったため不採用。二挿入を戻し、製品/src/tests/eng/CMake/flagsをmain221cbbcと同一へ復元して文書だけ収載する。混在で観測した短縮はmain適用済みの利益ではない。

規則はARC-001/002/004/007/008/011、CPP-003/008/011/012/016、QLT-001/002/004/008/010/012/013/014、CNF-001/002/003/004/009/012、D41。公開API/設定schema/fixture/基準/許容/抑制/allowlist変更なし、waiver none。

以下のOUTは実験時 `D:/NeNeNib/outputs/20261010-tint-clips`、WTは `D:/NeNeNib/worktrees/364-tint-clips`。恒久先は `D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-364` のfiles以下で元D相対pathを保つ。元path削除後も元raw metadataは書き換えない。通常旧版はsnapshot-362のfiles/worktrees/362-tint-glyphs/build/release-9218124/NeNeNib.exe（SHA ce0277729b358d2d66f2dda65d592b62e3a159eecb7dbe3e9b85a744d0a88f77）。新版通常Release0c91c99はSHA209a38fb976946745859d0b6854d8a11bf73dbc472d439a68a5316c548e8550d、通常DebugはSHAa741d8b653ce787a7be25d76ed499223d3e1e51bce746c370e894aed4cf954a1。

### 対象を限定した検証と原失敗

- 変更sourceのclang-format/source_checks/waiver_checks/whitespace、通常Debug configure/build `--target NeNeNib -j 2` は初回exit0。正規tidy/ASan/UBSan/no-recover、116段。strict二挿入と全他source/tests/eng/flagsの9gitobject一致、524sourceのbuild前後SHA一致を確認。`pwsh -NoProfile -File eng/build-release.ps1 -Ref 0c91c99` は通常Release初回exit0。command/実flags/元stdoutはOUT/implementation/{report.md,manifest.json,raw}、WT/out/364、OUT/release-0c91c99.log、WT/out/release/0c91c99.json。
- `python OUT/run-before.py` は新規edge49幅640〜688・一覧IME6・smoke3を初回exit0。`python OUT/run-gui.py --label debug/after --executable <各通常exe>` は本文27/本文IME4/一覧IME6/edge49とsmoke3を実行して全command正常終了した。これは比較結果の全成功を意味しない。`python OUT/verify-label.py --label debug` は成功、`--label after` は一覧比較でexit1。原validation-after.json/ime-palette/comparison.jsonはfailed/falseを保持。
- 本文27＋本文IME4のbefore31画像は#362 afterから再利用。同文書bytes/設定/字体/テーマ/DPI/刺激/道具/比較領域を照合し、絶対folder差が本文/statusへ表示されないことをsourceとbasenameから確認した。一覧emptyは候補locationに絶対folderが出るため旧画像を再利用せず新共通pathで採った。before metadata不変。compare-reused.pyのDebug/after31比較は差0、edge前後49は差0。最終列の固定muted色は49中16幅で到達/33幅で非到達。内部端点等値と完全左外側への実到達は未測。
- Release一覧03-other-compositionは6画素が一channel一段だけ異なった。他5場面は差0。6点はclient(1006,199)/(999,204)/(1024,204)/(1006,253)/(997,270)/(996,272)。本文入力欄の底185より下にあり、固定一回の所有者診断で全点が別process explorer.exeのApplicationFrameWindowだった。診断と元失敗Release PNGはSHA08a7b0d94e7b920127aaecc13313fbd8804fbef1e9bbee8b92d03a08dbeb12b2で完全同一。旧版/Debug PNGは別の同一SHA32c9aa2b0e75410174e5656f257630d70513a3dc06d45e9fe7c1a046a6096cc0。外部overlay表示領域の差という判断を#365へ分離したが、OSの具体的生成原因・同旧exeの揺れ・隠れたtarget画素は未実証。
- 原全画素条件はfalseのまま。mask/閾値/成功までの再撮影/元失敗の置換なし。独立review/ime-difference.mdの事後条件限定を許せるとした助言は、review/disposition-policy.mdで撤回された。AGENTSとQLT-010に従い、結果後に比較対象を狭めて今回を採用しない。QLT-012による別Issue分離と、採用条件未達を区別する。
- 本文IMEのother色0→204→0・Space accent285、一覧other0→83→0、open0復帰/文書破棄/設定不変を確認。正常Debugは86場面を正常実行し、再利用31比較は差0、一覧03の原PNGも旧版同一、edge末端着色は16有/33無だった。Debug全86の全画素比較を実施したとはしない。Releaseは85場面差0＋1場面6px差であり、「86場面同値」「全gate成功」としない。

### 一回の固定比較

i9-10850K/RTX3090/HP推奨、DPI120、1280×800、Cascadia Code13.5、system/guide on。ASCIIは`a\x01 `×1024、混在は`日a🖋\u200b `×1024、短行はASCII×2、CRLF。検索/a<CR>と先頭anchorを固定、暖機nN後の20n/Nを0.4秒間隔、ABBA×3で各120対応組を計画した。#362の混在3秒条件と直接比較しない。全36予定を一度だけ実施し、再試行/試料選別なし。

`python OUT/compare-364.py --before <9218124.exe> --after <0c91c99.exe>` は終了1。ASCII08-A（第9試行の旧版）がWM_CLOSE後15秒待っても終了せず、TimeoutExpired。raw marksは未出力、canonical driver.stopでそのowned processだけを停止した。ready/final画像はdirty dotと先頭余分文字様、最終Vim表示消失を示すが出所は未特定。ASCII固定120組は成立せずunmeasured。残り予定を記録し、成功試料だけでASCII改善率を補わない。

| 条件 | 前後中央値µs | 対応比中央値 | 短縮/同/遅延 | 状態 |
| --- | --- | --- | --- | --- |
| replaced-ascii-1024 | 算出しない | 算出しない | 集計しない | 旧版08-Aの失敗で固定比較不成立 |
| replaced-mixed-1024 | 20802.5→7054.0 | 0.338704158088 | 120/0/0 | 固定120組 |
| replaced-short-2 | 603.0→606.0 | 1.010831974533 | 53/2/65 | 固定120組 |

全36試行中35成功、1失敗。validatorは35試行の原marksを照合したが、必要36に足りずexit1/failed-or-unmeasured。欠測を除いて全比較成功とはしない。audit-rejected-experiment.pyはexit0/auditStatus completeで、製品受理のpassedではない。

`python OUT/validate-364-marks.py OUT/comparison --expect-trials 36 --output OUT/warmup-comparison.json` の結果と、成立した試行数は下記追記に記録する。成功用post-measurement-audit.pyは元のまま保存し、既知の未達を成功扱いに変更/実行しない。別audit-rejected-experiment.pyは元失敗が残ることと凍結source/exe/harness/GUI証拠/正式再利用/HEADの不変性だけを監査する。凍結environment.ps1の正常終了固定文をafterへ誤用せず、説明を訂正した別environment-rejected.ps1を使う。前後環境はsnapshotであり連続監視ではない。

正式single/burst200/burst200-16MiB/single-long-lineの#360成功4本は再実行していない。入力がtintへ到達せず、tint外のRendererとcore/application/adapters/tests/eng/flags/基準が同一であるsource証拠に基づく再利用。最終変更は製品全体を受理済みmain221cbbcへ戻す文書差分なので、元成功の再利用条件をcontentで確認し、製品試験を工程理由で繰り返さない。

初回prepareのGUI false assert、外部所有診断、read path/encoding/glob誤り、edge-inkの説明誤記と元script、比較09旧版の失敗、全画像/試料/marks/初期と最終reviewを保存。edge-ink集計RGB/16対33は不変で、説明だけBuiltinThemeの固定色へ訂正。詳細はjournalと各原stdout/JSON。任意文字/字体/DPI/IME/資源失敗、RSS/実確保回数、GPU完了時間は未測。

恒久snapshot-364へ全元先SHA照合して841files/28392986bytesを保存。manifest SHA `c54c1c9acb33f15ff5c8a330efda9cb6f8c3f3aedde58bc3e3d27673120de1fd`。独立review/acceptance.mdは不採用と記録の整合を確認。後続の文書/CI/main/復元proof/整理はacceptance-364。元候補commit/branchと全rawを残し、監査後にDの追加作業木・不要出力を削除する。
