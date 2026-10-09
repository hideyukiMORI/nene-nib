# ADR 0082 — 同じ固定処理を窓なしで前後比較する

- 状態: 受理（親設計サナの設計・2026-10-09）
- 日付: 2026-10-09
- Issue: #329
- 影響する規則: ARC-001 / ARC-002 / ARC-003 / ARC-007 / CPP-005 / CPP-007 / CPP-012 / CPP-016 / QLT-001 / QLT-012 / QLT-013 / QLT-014

## 文脈

#320〜#324 の閉じた処理の効果は、窓と描画全体の測定だけでは分離しにくい。
hide は他の作業がある機械でも、規約の範囲で固定入力の対応前後を比較するよう指定した。
正式な速度のゲートと異なる意味を持つ比較器を、製品に未検証の生成口や時計を足さずに用意する。

## 決定

1. 開発用 target `nib_perf_probes` を `tests/performance/`、module `performance_tests` に置く。
   依存は core / application / adapters_win32 / unit_tests。既存 `Editing` と Scripted ports はヘッダだけを借用し、unit の実行 target を link したり cpp を再コンパイルしたりしない。
   `eng/architecture.json` と `docs/PROJECT_LAYOUT.md` にこの境界を記録する。製品の依存と既存 unit の純粋さは変えない。CTest の既定には登録しない。
2. 時刻と測定ファイルの出力は既存 `Win32TimingAdapter` だけを使う。
   `Milestone` に `probe_started` / `probe_finished` を足し、網羅する名前の分岐も更新する。
   既存の入力と描画の節目を別の意味に転用しない。出力先は argv から共通 `to_utf16` を通して bind に渡す。test / core / application は時計やファイルを読まない。
3. 最初の固定 workload は四つ: `controller-open-utf8-16mib`（200000 CRLF 行・16800000 bytes）、`buffer-from-utf8-16mib`（同じ本文）、`controller-insert-200`（通常の新規文書・VisibleLines 30・200回のInsertTextと戻りframe生成）、`display-line-long`（既存長行ベンチの日本語文の90反復を含む1行）。
   開く場面は毎回新しい Editing と ScriptedFiles を準備し、既存タブの no-op を測らない。現在 main と #320 / #321 で共通の API だけを呼ぶ。後続用だけの API は足さず、旧アルゴリズムもコピーしない。
4. 入力生成・ポート準備・期待値との照合・checksum は区間外。処理の戻り値を保持して必ず観測する。
   controller の全文確認は区間後の既存 SaveDocument と ScriptedFiles の記録で行う。
   反復数は CLI で事前確定（既定20）。warmup を bind 前に一回実行し集計から外す。各反復に開始/終了を対応させ、4096 marks の上限を越える入力を拒否する。誤った本文・行数・選択などは非0で終わる。
5. `eng/compare-probes.py` は before / after exe・ref・commit・workload・output・反復数を明示して使う。
   同じ設定の ABBA ブロックを既定3回（12 process・各内部20 sample）実行する。subprocess は窓を出さず、各 process の測定先は独立させる。
   全値と対応する ratio・中央値・range を保存する。途中で順や試行数を変えず、外れ値除外もしない。欠測・timeout・失敗終了・不正 JSON / marks・checksum 不一致は判定不能として非0で返す。
6. exe の SHA256、ソース ref / commit、build 時の compiler / configuration、入力 bytes と実際の入力の FNV-1a64、反復数・順序・warmup・出力 checksum を記録する。
   checksum の一致は性能の合格を意味しない。独自の基準値・自動調整・成功までの再試行は作らない。
7. 同じ harness を main と候補に載せて比較する。#329 は main 起点で完結し、候補への取り込みと長い実測は親が行う。
   開発 target の Debug / Release は固定 `eng/toolchain.ps1` と同じ CMake 警告・静的解析を使い、target だけ並列2で build する。
   製品 exe 専用の `eng/build-release.ps1` は使わず、この開発 target の CMake Release build を製品 Release と呼ばない。

## 検証と限界

道具の対象試験で、短い自己比較・壊れた marks・片側の失敗・checksum の違いを確認する。
変更 target の Debug / Release、対象の規約・依存・整形・シンボルを確認する。実機窓・正式 speed・全件回帰・coverage・oracle は実装席では実行しない。
この比較は UI / GPU / 実ファイル I/O の遅延を測らず、正式 QLT-014 を置き換えない。小さい差だけで採用を決めない。既存 perf-reference と許容は変えない。
