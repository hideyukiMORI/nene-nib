# 速さの基準値の採用 — 2026-10-18（Issue #644・施主決定 D51 / D52）

施主決定 D51（目標値は別に持ち、ゲートの基準値は実測のまま）と D52（巨大 1 行は 1 回測ってから決める）に従い、2026-10-18 の起動時に 1 回だけ測って `eng/perf-reference.json` のこの機械の欄を書き換えた。許容（25%・下限 2 ms）と判定する 8 本は変えていない（QLT-010 / QLT-014・ADR 0138）。前の基準値は 2026-09-15 の実測（[speed-reference.md](speed-reference.md)）。

## 条件

| 項目 | 値 |
|---|---|
| main | `b3e6f01`（#642 の後・clean） |
| exe | `build-release/NeNeNib.exe`（`eng/toolchain.ps1` → `cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release` → `--target NeNeNib`・148/148・11:35） |
| exe の SHA-256 | `9E0CD025DA05AAC91D5286214BF45EECE626B85A0D8A251C5BEAB78442BCE9DA` |
| 機械 | `bc8a356f37c68491`（i9-10850K・RTX 3090・DPI 120） |
| 負荷 | 測る前 3 回: 2.2 / 6.6 / 3.5%・直前 5.1%。ほかの席とビルドは無し。Codex のプロセスは居たが作業していない（hide の申告）。hide は計測の間打鍵しない（了承 11:30 頃） |
| 道具 | `python eng/measure-speed.py --check --repetitions 5`（8 本）・`--record --bench <名前> --repetitions 5 --grid-answer yes` / `no`（巨大 1 行 4 回） |
| 記録 | `out/speed/2026-10-18T02-37-24Z.json`（8 本）・`02-37-36Z`（giant open はい）・`02-37-40Z`（同 いいえ）・`02-37-52Z`（giant 1 打鍵 はい）・`02-38-04Z`（同 いいえ）。ログは `D:/NeNeNib/outputs/20261018/gate-main-b3e6f01.log`・`giant-*.log` |

## 既存 8 本（`--check` → `Speed: 8 benches checked, 0 regression(s), 0 unmeasurable`）

中央値・最小・最大は ms。5 試行は測った順。「差」は前の基準値（2026-09-15・16 MiB 連打は 2026-09-23・面は 2026-10-02・長行は 2026-10-04）に対する中央値の差。D51 の目標は起動 50・窓 40・単打 1・連打 3・open 60・16 MiB 連打 3 ms。

| ベンチ | 前の基準値 | 新 中央値 | 最小 | 最大 | 差 | 5 試行 | D51 |
|---|---:|---:|---:|---:|---:|---|---|
| `startup-first-frame` | 191.488 | **41.886** | 39.661 | 55.294 | −78% | 55.294, 41.886, 40.016, 39.661, 43.515 | 50 の内 |
| `startup-window-shown` | 34.933 | **32.376** | 30.942 | 41.975 | −7% | 41.975, 32.376, 30.942, 30.953, 33.067 | 40 の内 |
| `key-to-frame-single` | 0.906 | **0.641** | 0.630 | 1.289 | −29% | 1.289, 0.630, 0.738, 0.635, 0.641 | 1 の内 |
| `key-to-frame-burst-200` | 2.695 | **2.161** | 1.839 | 2.336 | −20% | 1.839, 2.161, 2.126, 2.336, 2.171 | 3 の内 |
| `open-large-file-16mib` | 249.783 | **49.789** | 47.528 | 63.719 | −80% | 63.719, 50.021, 49.789, 47.528, 48.528 | 60 の内 |
| `key-to-frame-burst-200-16mib` | 7.411 | **2.340** | 2.282 | 2.555 | −68% | 2.348, 2.340, 2.335, 2.555, 2.282 | 3 の内 |
| `key-to-frame-palette-5000` | 2.630 | **1.729** | 1.662 | 2.108 | −34% | 1.729, 1.686, 1.662, 1.730, 2.108 | 目標なし |
| `key-to-frame-single-long-line` | 5.431 | **1.574** | 1.488 | 1.999 | −71% | 1.488, 1.574, 1.807, 1.554, 1.999 | 目標なし |

- 目標のある 6 本は全部 D51 の内。起動 41.9 / open 49.8 は 2026-10-15 の #487 の exe の 40.5 / 52.2 と同じ水準で、速度向上デーの鎖が main に揃っても崩れていない。
- 起動の内訳（中央値 ms）: origin 10.2 | document_opened 0.5 | window_created 3.5 | backdrop_applied 1.0 | window_shown 17.2 | device_created 1.2 | swap_chain_created 1.7 | composition_bound 0.7 | context_created 0.9 | text_formats_created 1.9 | frame_presented 3.3。`device_created` が 1.2 ms なのは WARP（ADR 0132）で、本物の device は裏で作っている（ADR 0131）。
- 16 MiB open の内訳: `document_opened` 9.9 ms・ほかは起動と同じ。
- 1 試行目が 5 本で最大（起動 55.3・窓 42.0・単打 1.29・open 63.7）。最初の起動のキャッシュの冷えで、中央値には効かない。
- #460（16 MiB 連打の既存の基準超過 15.3 ms）は #474 / PR #615 の後 2.34 ms で、旧基準 7.411 の内。この計測で閉じる。

## 1 行 16 MiB の 2 本（ADR 0138・`--record --bench`・各 5 試行・ms）

| ベンチ | 答え | 中央値 | 最小 | 最大 | 5 試行 | 2026-10-16 の鎖の先頭 |
|---|---|---:|---:|---:|---|---:|
| `open-giant-line-16mib` | はい（格子） | **129.641** | 120.820 | 161.971 | 161.971, 120.820, 129.641, 160.277, 127.420 | 315.4 |
| `open-giant-line-16mib` | いいえ（窓） | 161.653 | 124.072 | 164.071 | 164.071, 161.653, 127.070, 162.129, 124.072 | 283.9 |
| `key-to-frame-single-giant-line-16mib` | はい | **1.232** | 0.974 | 1.352 | 1.210, 0.974, 1.315, 1.232, 1.352 | 1.056 |
| `key-to-frame-single-giant-line-16mib` | いいえ | 2.118 | 1.678 | 2.177 | 2.177, 1.678, 2.135, 2.118, 2.098 | 2.182 |

- 内訳（はい）: `document_opened` 28.4 | `window_shown` 15.9 | `frame_presented` 2.5 | **`body_presented` 62.2**（問いに答えてから本文の frame まで）。いいえは `body_presented` 90.1。
- 開く 2 本の 5 試行は 2 つの山（121〜130 と 160〜164）に分かれる。中央値はどちらの山にも落ち得る（はい は下の山・いいえ は上の山に落ちた）。山の差は約 35 ms で許容 25%（32 ms）とほぼ同じなので、将来この 2 本を判定の 8 本に移すなら試行を増やすか山の原因を先に見る。今回は判定に載せない（ADR 0138 決定 4・`OPTIONAL_BENCHES` のまま）。
  - 原因（2026-10-19・#689）: 山は表示形式の問いが答えを待つ時間（≈45 / ≈95 ms・計測器の 50 ms 刻みの見張りと問いの窓の生成）で、製品の本文の速さではなかった。10 試行で待ちを除くと 55〜75 ms に収まる。
  - #689 から道具はこの待ち（節目 `question_shown` → `question_answered`）を値から引くので、この表の開くの値はそれより前の定義（待ちを含む）。基準値の採り直しは別の記録。
- 2026-10-16 の D53 の鎖の先頭（#552 / #557・315.4 / 283.9）より開くが速いのは、その後に main に入った起動の鎖（#618 → #620・device の先行と WARP の最初の frame と DLL の遅延読み込み）の分。1 打鍵は同等。
- 「ひどい」値ではないので D52 のとおり **「はい」の 2 本を採用**した。「いいえ」は記録だけ（道具の `--grid-answer` の既定が `yes` で、基準値は 1 つの答えで持つ）。

## 書いたもの

- `eng/perf-reference.json`: この機械の欄を `--adopt --values out/speed/2026-10-18T02-37-24Z.json`（8 本・`recordedAt` 2026-10-18T02-37-24Z）で書き換え、`--adopt --bench open-giant-line-16mib --values …02-37-36Z.json` と `--adopt --bench key-to-frame-single-giant-line-16mib --values …02-37-52Z.json` で 2 本を足した。`benches` の説明・`tolerance`・CI の機械の欄は不変。
- 新しい許容上限（25%・下限 2 ms）: 起動 52.4・窓 40.5・単打 2.64・連打 4.16・open 62.2・16 MiB 連打 4.34・面 3.73・長行 3.57 ms。起動と open は下限 2 ms が効かず 25% だけなので、以前（191 ms の 25% = 48 ms）より絶対の幅は狭い。混んだ機械では落ちやすくなるが、それは「測る前に機械を静かにする」（CLAUDE.md §2）で守り、許容を広げない（QLT-010）。

## 検証

- `python eng/test-conformance.py` → 340 tests OK（`perf-reference.json` の説明の鍵と `BENCHES` の並び・`adopt_one` が他の値を動かさない契約を含む）。
- `eng/prove-gates.py` の `prove_speed_reference`（QLT-014 の反例の証明。基準値の複製を 1 本だけ締めて `--check --values` が非 0・戻すと 0）を単独で実行した結果は下の節。
- 製品のコード・試験・fixture は変えていないので、ビルド・ctest・`-Full` は実行しない（QLT-001 / QLT-012・ADR 0021）。CI は `eng/` を触るので merge 直前に `check` と `build-and-test` の 2 job を見る。
