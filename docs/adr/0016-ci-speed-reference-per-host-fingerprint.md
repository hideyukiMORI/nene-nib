# ADR 0016 — CI の速さの基準値は host の指紋ごとに持ち、基準値の無い host では記録だけにして黙らず、記録を artifact で残す

- 状態: 受理
- 日付: 2026-09-18
- Issue: #47
- 影響する規則: QLT-014 / QLT-013 / QLT-007

## 文脈

ADR 0011 の決定 4 は基準値を機械の指紋（CPU 名・GPU・DPI）ごとに持ち、「CI の基準値は同じ CI 機での値が数回たまってから別の Issue で決める」と留保した。
CI の `check`（GitHub Actions・`windows-2022`）は 16 run で速さを測ったが、`--check` は毎回 `no reference for this machine; recorded only` で終了 0 で、QLT-014 は CI では planned のまま。
16 run のログを並べた（`docs/quality/speed-reference.md`「CI の記録」）:

- **指紋は 6 種類**。ランナーの host の CPU 世代が混ざる（AMD EPYC 7763 が 10/16、EPYC 9V74 が 2、Xeon 6973P-C / Platinum 8370C / EPYC 9V45 / Platinum 8573C が各 1）。GPU は全部 `Microsoft Hyper-V Video`（WARP）・96 DPI
- **CPU 世代の差は許容 25% を超える**（起動 25.9 → 38.0 ms、16 MiB 61.9 → 108.9 ms）。速い host の値を基準にすると遅い host の run が偽の退行で落ち、逆なら速い host の退行を見逃す
- **同じ指紋の中の揺れは 1.10〜1.27 倍**（EPYC 7763・#24 以降の 7 run）で 25% / floor 2 ms に収まる
- `startup-first-frame` と 16 MiB は #24（ADR 0013）で意味が変わった。#24 より前の 6 run は材料にならない
- CI の記録（`out/speed/*.json`）は `.gitignore` と artifact 無しで 1 つも残っておらず、値はログの表示行（小数 3 桁）からしか拾えない
- 打鍵 2 本（1.2〜3.0 ms）は floor 2 ms に飲まれ、CI では 1.75〜2.4 倍まで落ちない
- CI で退行として見えるのは `window_shown`（11〜17 ms）と `document_opened`（16 MiB で 32〜64 ms）で、実機の主費用 `device_created`（NVIDIA 160 ms）は CI では 3〜4 ms

## 決定

**CI の基準値は施主の実機と同じ形で host の指紋ごとに持つ。いま足すのは EPYC 7763 の指紋 1 つで、値は同じ指紋の複数 run の中央値の中央値。基準値の無い host では記録だけにするが、そのことを 1 行で言って黙らない。CI の記録は artifact で残す。**

1. **指紋の計算は変えない**（CPU 名・GPU・DPI）。CPU 世代で値が 25% 以上違う以上、CPU 名を鍵から外して 1 つの基準値にすると、許容を広げるか偽の退行を受け入れるかしかない。「同じ機械での相対退行」（ADR 0006 / 0011）を守る
2. **EPYC 7763（`e7a87d5b6ac1e14b`）の項を `eng/perf-reference.json` に足す。** 値は #24 以降の 7 run（35121845572 / 35208906192 / 35209555298 / 35216632797 / 35217895642 / 35219937347 / 35354289264）の run ごとの中央値の**中央値**、`minimumMs` / `maximumMs` はその 7 つの中央値の最小・最大。
   1 run の `--adopt` ではない（CI の 1 試行目はコールドスタートの外れ値 240〜1485 ms を含み、1 run の最大値は基準の記録として誤読される）。項の `note` に出典の run を書く。許容は共通の 25% / floor 2 ms のまま
3. **基準値の無い host は記録だけ・終了 0 のまま、ただし黙らない。** `--check` は `Speed: no reference for <fingerprint> (<cpu>); recorded only -- QLT-014 is not judged on this host` を出し（ダッシュは ASCII の `--`。U+2014 は cp932 に無く、施主の机で `print` が落ちてゲートが「退行」と言う）、`check.ps1` のまとめの行にも出す。CI の 4 割の run を落とすのは共有ランナーの都合であって退行ではない。落とす選択（`--require-reference`）は却下ではなく保留（下）
4. **`check.yml` は `out/speed/*.json` を `actions/upload-artifact`（保持 90 日・名前 `speed-records`）で上げる。** 他の指紋の基準値をためる道はこれだけ。採用は人が artifact を取って `--adopt --values <記録>`（複数 run なら中央値の中央値を手で書き、出典を `note` に）
5. **正直に書く**: QLT-014 は CI では「指紋 `e7a87d5b` の run で active（16 run 中 10）・他の host は記録だけ」。打鍵 2 本は CI では floor 2 ms に飲まれて判定にならない（planned。floor を機械ごとにするかは値がたまってから）

## 強制

- QLT-014（CI・指紋 `e7a87d5b6ac1e14b`）— **active**（この Issue で。`check.ps1` → `measure-speed.py --check` が基準値と比べて落ちる。反例は既存の `prove-gates.py` の `prove_speed_reference` がそのまま通る）
- QLT-014（CI・他の指紋）— **planned**（記録だけ。artifact がたまったら指紋ごとに項を足す）
- QLT-014（CI・打鍵 2 本）— **planned**（floor 2 ms に飲まれる）

## 結果

得られるもの: CI の 6 割の run で 5 本の退行判定が動く。記録が残るので、他の指紋の基準値と退行の追跡ができる。「判定していない run」がログで分かる。
失うもの: 4 割の run は判定なし。CI の判定は実機の主費用（GPU ドライバ）を映さない（CI で見えるのは CPU 側の `window_shown` と `document_opened`）。16 MiB は同じ指紋でも #29 の外れ値（108.9 ms・上限 109.2）が出るので、机の荒れで落ちる回がありうる（落ちたら run をやり直す。基準値を上げるのは ADR の判断・決定 5 のまま）。

## 却下・保留した選択肢

| 選択肢 | 判断 |
| --- | --- |
| CPU 名を鍵から外して CI を 1 つの基準値にする | 却下。世代差 47〜76% を許容に入れるとゲートにならない |
| 機械ごとの許容（`machines.<key>.tolerance`） | 保留。同じ指紋の中の揺れは 25% に収まっている。要るのは 16 MiB の外れ値が続いたとき |
| 基準値の無い host で落とす（`--require-reference`） | 保留。ランナーの CPU 世代は選べず、4 割の run を落とすのは退行の検出ではない。指紋が 2〜3 つ揃えば見直す |
| CI で `--adopt` して commit する | 却下。`contents: read` のまま。基準値を書くのは人（ADR 0011 の決定 5） |
| 1 run の `--adopt` で CI の項を書く | 却下。1 試行目のコールドスタートで最大値が汚れ、1 run の機械の荒れがそのまま基準になる |

## 関連

ADR 0006（速さのゲート）・ADR 0011（計測・指紋ごとの基準値）・ADR 0013（起動の意味の変化）・Issue #30 / #36（計測不能）。
