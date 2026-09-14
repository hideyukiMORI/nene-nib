# ADR 0001 — 厳格さは機械で強制する

- 状態: 受理
- 日付: 2026-09-15
- Issue: #1
- 影響する規則: すべて

## 文脈

本リポジトリの規約は、AYANE 厳格規約ポリシー（施主の private ワークスペースにある `POLICY.md` / `INIT_PROCEDURE.md` / 雛形）と、
先行する 5 リポジトリ（NENE-PIXEL＝Kotlin / nene-recall＝Go / xi-tools＝Rust / NeNeCommander＝C# / NeNeClock＝Java）、
そしてテンプレの先行 2 適用（nene-loupe＝C++23 / MSVC、nene-folio＝C23 / clang-cl）で確立された考え方を
C++23 (clang-cl) へ持ち込んだものである。共通する中核は 1 つに要約できる。

> **一つのことを実現する方法を 1 つに固定し、そのことを人の記憶ではなく機械に守らせる。**

規約は、破ったときに何も起きなければ、時間とともに必ず腐る。

### C++23 (clang-cl) で実測したこと

2026-09-15 に MSVC 19.44.35228 と clang-cl 19.1.5（同じ Visual Studio Build Tools に同梱）で 114 記録を実測した。
再現は `pwsh -NoProfile -File ./eng/measure-language.ps1`、全記録は [phase0-results.json](../quality/phase0-results.json)。
Loupe の 19 ケース（M1〜M8）はそのまま再現し、同じ結果だった。

```
M1-exhaustive (cl /we4061 /we4062): exit 2 / C4062  列挙子 'Mode::hex' はハンドルされません
M1-clang-missing-case (clang-cl -Wswitch-enum): exit 1 / enumeration value 'hex' not handled in switch
M1-clang-covered-default (clang-cl): exit 1 / default label in switch which covers all enumeration values
M1-out-of-range-hole (cl / clang-cl): exit 0   ← static_cast<Mode>(99) は両方通る
M2-private / M4-private (cl): exit 2 / C2248   (clang-cl): exit 1 / is a private member
M2-cstyle-cast-clang (-Wold-style-cast): exit 1   M2-const-cast-clang-hole: exit 0   M2-mutable-hole: exit 0
M3-uninitialized (cl): C4700   (clang-cl -Wuninitialized): exit 1
M3-null-clang-hole: exit 0 → T2 clang-analyzer-core.NullDereference: exit 1
M3-optional-hole (*value): exit 0 → T3 bugprone-unchecked-optional-access は value() だけ拒否、operator* は通る
M5-time / M6-win32: exit 0 → L1 llvm-nm: U _Xtime_get_ticks / L2 llvm-nm: U __imp_GetTickCount
TH1 llvm-nm: U _beginthreadex / _Mtx_lock / _Cnd_wait / __imp_CreateThread   TH2: std::atomic はシンボルを残さない
M7-suppression-hole (cl #pragma warning(disable)): exit 0   M7-suppression-clang-hole (#pragma clang diagnostic ignored): exit 0
K1-auto-cast-msvc-hole (cl): exit 2 / C3537   K1-* (clang-cl): 全部 exit 0
S1-avx2-without-target-clang: exit 1 / requires target feature 'avx'   S3-avx2-without-arch-msvc-hole: exit 0
A1-asan (cl・clang-cl): AddressSanitizer: heap-buffer-overflow   A2-ubsan (clang-cl): runtime error
D1-directx-static-clang-cl: exit 0 / imports d2d1 DWrite dxgi d3d11 dcomp dwmapi KERNEL32
R1: /MT の exe は KERNEL32.dll しか import しない
```

| 先行事例の規則 | C++23 (clang-cl)（実測） |
| --- | --- |
| 不正状態を表現不能に・網羅性 | 不十分。`-Wswitch-enum` と `-Wcovered-switch-default` で分岐漏れと網羅済み `default` を拒否（M1-clang-*）。範囲外の enum 値はキャストで作れる（M1-clang-out-of-range-hole） |
| 公開状態は不変 | 不十分。private への代入はコンパイルエラー（M2-private-clang）。C スタイルキャストは `-Wold-style-cast` が拒否（M2-cstyle-cast-clang）。`const_cast`（M2-const-cast-clang-hole）は clang-tidy `pro-type-const-cast`（T4）が拒否。`mutable` と公開 aggregate（T1-tidy-aggregate-hole）は通る |
| `null` の意味は一つ（ゼロ値・未初期化の穴） | 不十分。未初期化は拒否（M3-uninitialized-clang）。`0` をポインタにするのは `-Wzero-as-null-pointer-constant`（C6）。`nullptr` の逆参照はコンパイルが通り（M3-null-clang-hole）、clang-analyzer が拒否する（T2）。空の `optional` の `operator*` は通る（T3-*-star-hole）。値初期化 `Id{}` は通る（M3-value-init-hole） |
| 非公開コンストラクタ＋唯一のファクトリ（迂回経路） | ある（M4-private-clang）。検証済み値のコピーは可能（M4-copy）。`malloc`＋`static_cast` の偽造は通る（M4-forge-hole）ので、中核の許可シンボルから `malloc` を外す |
| 中核の決定性を構文的に塞げるか | 言語では無い（M5-time-clang）。リンカ段の `llvm-nm` で `_Xtime_get_ticks` / `_Query_perf_counter` / `_Random_device` / `__imp_GetTickCount` を捕まえる（L1 / L2）。名前でなくシンボルを見る |
| 依存方向をビルドが拒むか（標準同梱の枠組みの漏れ） | 言語では無い（M6-win32-clang）。CMake の宣言グラフ＋File API＋`llvm-nm` の `__imp_*` / `__std_fs_*` 検査で補う |
| 抑制を言語で封じられるか | 無い。`#pragma clang diagnostic ignored` で `-Werror` を抑制できる（M7-suppression-clang-hole）。waiver 台帳を持つ |
| baseline を作る経路が塞がっているか | 言語では無い。ファイル先頭の pragma で全面抑制できる（M8-file-suppression-hole）。CNF-003 / CNF-005 で補う |
| 並行性を一種類に固定できるか（Nib 固有） | 不十分。スレッド生成・mutex・条件変数はリンカに見える（TH1）。`std::atomic` は見えない（TH2）ので、ヘッダの字句検査（CNF-009）で補う |
| SIMD の許可範囲を機械で強制できるか（Nib 固有） | ある。target feature の無い関数の組み込み関数はコンパイルエラー（S1）。cl には無い（S3） |

**そして、いま実装が空である。** 規則を後から被せる場合に必要になる baseline が、今は要らない。
違反ゼロから始められる時点は今しかない。

## 決定

**すべての規範規則に「機械強制の状態」を持たせ、その状態を文書の側で機械検査する。**

1. 規則の正本を `docs/ARCHITECTURE_CONSTITUTION.md` / `docs/CODING_RULES.md` / `docs/QUALITY_GATES.md` に置く。
   すべての規則に ID を与え、状態を **active / planned / 不能 / 不採用** で明示する。**planned を active と書かない**
2. `pwsh -NoProfile -File ./eng/check.ps1` を唯一の入口とする。CI は同じコマンドを呼ぶだけにし、CI 側にしか無い検査を作らない
3. baseline を持たない。例外は期限付きの狭い waiver だけ
4. 抑制には理由と規則 ID を要求する（台帳を持つ。C++ は抑制を言語で封じられない＝M7）
5. ゲートを弱める変更は ADR を要する
6. 道具の版は eng/tool-versions.json だけが決める。2 か所に書かない
7. **ゲートは意図的な違反で発火することを証明してから active と書く**（`docs/quality/gate-proofs.md`）
8. 規約検査（`eng/conformance.py`）は依存ゼロで書く

## 強制

- **planned**: CNF-006（文書整合）・CNF-005（baseline 禁止）・CNF-004（waiver 期限）。実装後に active へ
- **不能**: 「`planned` を勝手に `active` と書き換えないこと」そのもの。マトリクスの行と実装の対応は、いまは人が見るしかない

## 結果

得られるもの:

- 規約が自分について嘘をつく経路が塞がる
- 新しい貢献者（人でも AI でも）が「何がいま守られているか」を 1 か所で読める
- 規約を緩めるコストが上がる

失うもの:

- 文書のメンテナンスコスト。規則を足すたびに 3 か所（本文・マトリクス・実装）が同期を要求される
- 検査の実装そのものの保守
- 書き味の制約: 中核は `new` を所有者経由でしか使えず、`std::optional` は `value()` で読み、COM の生ポインタと Win32 のメッセージを
  閉じた意図と表示値へ変換する境界が要る。ワーカーは 1 系統だけで、共有可変状態を持てない

**正直に記録しておくこと:**

- 🔴 メモリ安全（境界外・二重解放・解放後使用）は言語では塞げない。ASan / UBSan の**検出**を単体テストに常時付けるが、検出は防止ではない（CPP-016）。
  範囲外 enum・`mutable`・`const_cast`・空の optional の `operator*`・`malloc` 偽造・pragma 抑制・`std::atomic` による共有可変状態も
  言語では塞げず、静的解析・リンカ段・字句検査と規約で狭めるだけである
- **本 ADR の厳格さは、実装がまだ空である今だからこそ無傷で導入できた。** 緑であることは、規則が良いことの証明ではない。
  検査対象がまだ小さいことの結果でもある。実装で規則が邪魔になったとき、緩めるのではなく **ADR で判断を残すこと**が本 ADR の眼目である

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 散文の規約だけを置く | 守られているかを確かめる手段が無く、時間とともに必ず腐る。**本 ADR の出発点そのもの** |
| 既製 lint の既定セットだけで済ませる | 既製ルールは「C++23 (clang-cl) として危ういこと」を見るが、「NeNe Nib として守るべきこと」は見られない。規則と lint の対応が付かず「なぜ有効か」を説明できない |
| 全 lint の一括有効化 | 相互に矛盾する lint が同時に入り、規則ではなく道具の機嫌に従うことになる |
| baseline を作って既存違反を後回しにする | 新規リポジトリなので既存違反が無い。ここで baseline を許すと、以後の違反も同じ入口から入ってくる |
| 実装が入ってから導入する | 既存違反を凍結する baseline が必要になる。今なら違反ゼロで始められる |
| 規約を `CLAUDE.md` にだけ書く | 人間と AI の遵守に依存する。前例（nene-recall）で実証済みの失敗——規約は CLAUDE.md にあったが、守っていたのはテスト 10 ケースだけで、新しく書かれるコードには及んでいなかった |
| 規則 ID を付けず散文で参照する | 「どの規則の話か」がレビューのたびに揺れる。ID があると tooling・ADR・PR・waiver が同じ語を指せる |
| Loupe の実測を再現せずに写す | 同じ言語でも道具の版が変われば結果が変わりうる。19 ケースを再現して同じ結果を得てから、Nib の分だけ足した |
| MSVC `cl` を C++ のコンパイラにする（Loupe と同じ） | SIMD を無条件に通す（S3）、`auto(x)` が無い（K1-auto-cast-msvc-hole）、`/showIncludes` が日本語（H1）。詳細は [ADR 0003](0003-cpp23-clang-cl-foundation-and-measured-limits.md) |
| `dumpbin` の出力をソース上の名前で grep して決定性を検査する | `system_clock::now()` は `_Xtime_get_ticks` に写像され、名前では見つからない（L1）。`llvm-nm --undefined-only` のシンボル名を許可リストと完全一致で照合する |

## 関連

- 先行: NENE-PIXEL `docs/ADR`、nene-recall ADR 0010、xi-tools ADR 0001、NeNeCommander ADR-0001、NeNeClock ADR 0001、
  nene-loupe ADR 0001 / 0003、nene-folio ADR 0001 / 0003
- 道具の選定と実測できた限界の詳細: [ADR 0003](0003-cpp23-clang-cl-foundation-and-measured-limits.md)
