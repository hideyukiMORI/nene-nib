# C++23 (clang-cl) コーディング規約 — NeNe Nib

> Status: normative（規範）/ 2026-09-15 初版
> 判断の根拠は [ADR 0001](adr/0001-strictness-is-mechanically-enforced.md) と [ADR 0003](adr/0003-cpp23-clang-cl-foundation-and-measured-limits.md)。
> 本書は C++23 (clang-cl) を本リポジトリで承認された部分集合に狭めるためのものであり、
> ここが沈黙している領域は公式の C++23 標準に従う。

読み方（**active / planned / 不能 / 不採用**）と強制の層は
[ARCHITECTURE_CONSTITUTION.md](ARCHITECTURE_CONSTITUTION.md) 第 0 節と同じ。実測のケース番号は
[phase0-results.json](quality/phase0-results.json) の `id`。

---

## 1. 型と状態（CPP-0xx）

### CPP-001 — 境界でのプリミティブ執着を禁じる

識別子・寸法・版番号・行番号・桁位置のように**単位や不変条件を持つ値**は専用の型にする。生の `int` / `size_t` / `std::string` を
深いところまで運ばない。検証は生成時に行う。

- 機械強制: **planned**（「専用型にすべき値かどうか」はレビュー事項）
- 補助: CPP-007（唯一のファクトリ）により、型を作れば必ず検証される

### CPP-002 — 閉じた選択肢は閉じた型で表し、網羅性検査を殺さない

モード・状態機械・キーの種別を boolean の組み合わせ・マジック整数・裸の文字列で表さない。`enum class` で表し、
分岐に `default` / `else` / `_` を書かない。**書いた瞬間に検査は死ぬ。** 範囲外の enum 値はキャストで作れる（M1-clang-out-of-range-hole）ので、
enum は境界で範囲検査してから受け取る（CPP-007）。

- 機械強制: **planned** → `-Wswitch-enum`（分岐漏れ）と `-Wcovered-switch-default`（網羅済みの `default`）を `-Werror` で（M1-clang-*・実測 2026-09-15）。if/else の意味分類は未完了

### CPP-003 — 公開状態は不変

公開フィールドを持たない。可変コレクション・配列・可変ビルダを外へ返さない。`const_cast` と `mutable` を書かない。
外から受け取った可変データは防御的に複製して所有する。

- 機械強制: **planned** → private への代入はコンパイルエラー（M2-private-clang）。C スタイルキャストは `-Wold-style-cast`（M2-cstyle-cast-clang）。`const_cast` は clang-tidy `cppcoreguidelines-pro-type-const-cast`（T4）。公開 aggregate はメソッドを持てば clang-tidy `misc-non-private-member-variables-in-classes`（T1-tidy-public-with-method）が拒否するが、メソッドの無い aggregate（T1-tidy-aggregate-hole）と `mutable`（M2-mutable-hole）は言語でも lint でも通る。`mutable` は CNF の字句検査で拒否する予定。公開 aggregate（`RgbColor` / `Palette`）に `= default` の `operator==` をメンバーで書くと lint が落ちるので、比較は非メンバーで書く（Issue #3 で実測）

### CPP-004 — `null` の意味は一つ

`nullptr` が意味してよいのは「省略可能な値が無い」だけ。無効・未読込・失敗・未知・削除済みを表さない。公開 API はポインタで
「無い」を返さず、`std::optional` か結果型で返す。**`std::optional` は `value()` か `value_or()` で読み、`operator*` / `operator->` で
読まない**（空の optional の `operator*` は clang-tidy が見ない＝T3-tidy-unchecked-optional-star-hole）。`0` をポインタにしない。

- 機械強制: **planned** → 未初期化の読み出しは `-Wuninitialized`（M3-uninitialized-clang）。`0` → ポインタは `-Wzero-as-null-pointer-constant`（C6）。`nullptr` の逆参照はコンパイルが通り（M3-null-clang-hole）、clang-tidy `clang-analyzer-core.NullDereference` が拒否する（T2）。引数の optional の `value()` は `bugprone-unchecked-optional-access`（T3-tidy-unchecked-optional-value）。`operator*` の禁止は CNF の字句検査で拒否する予定

### CPP-005 — 期待される失敗は例外で表さない

検証エラー・見つからない・拒否・非互換・device lost は **`std::expected<T, E>` か閉じた `enum class` の結果型**で返す。
結果を返す関数は `[[nodiscard]]` を付け、呼び出し側は必ず分岐する。例外はプログラム上の欠陥・下位層の想定外に限り、
広い `catch` を書かない。OS のコールバック（窓手続き・COM）は `noexcept` とし、例外を OS の枠へ流さない。

- 機械強制: **planned** → `[[nodiscard]]`＋`-Wunused-result` で戻り値の無視を拒否（C7-nodiscard-clang）。結果型の選択と `catch` の広さはレビュー事項

### CPP-006 — 汎用データバッグを禁じる

`void *` / `std::any` / 文字列キーの連想配列 / 意味を持つ値の `std::tuple` / `std::pair` で型を代用しない。名前付きの `struct` を作る。
`void *` を許すのは Win32 のコールバック引数と COM の `IID_PPV_ARGS` を境界で受ける場所だけである。

- 機械強制: **planned**（`std::any` と core / application の `void *` は CNF の字句検査で拒否する予定）

### CPP-007 — 構築が不変条件を守る

不変条件を持つ型はコンストラクタを非公開にし、生成経路を**唯一のファクトリ**に集約する。ファクトリは想定内の不正入力に例外を投げず、
結果型を返す。

- 機械強制: **planned** → 非公開コンストラクタは直接構築できない（M4-private-clang）。検証済み値のコピーは許可される（M4-copy）。`malloc`＋`static_cast` の偽造（M4-forge-hole）と値初期化 `Id{}`（M3-value-init-hole）は言語では塞げず、中核の許可シンボルから `malloc` を外し（[ADR 0003](adr/0003-cpp23-clang-cl-foundation-and-measured-limits.md)）、不変条件を持つ型に既定コンストラクタを置かないことで狭める

---

## 2. 構築と可視性

### CPP-008 — 可視性は最小

既定は非公開。他から実際に使うものだけ公開する。テストのためだけに公開しない。翻訳単位に閉じるものは無名名前空間に置く。

- 機械強制: **planned**

### CPP-009 — 言語マジックを制限する

関数風マクロ・`reinterpret_cast`・`dynamic_cast` と RTTI・`goto`・インラインアセンブリ・`#pragma`・`setjmp` / `longjmp`・
可変長引数関数の新設・ビルド時コード生成を禁じる（使うなら ADR）。オブジェクト風マクロは `#define` より `constexpr` / `enum class` を使う。
COM の `__uuidof` / `IID_PPV_ARGS` は `src/ui/win32` と `src/adapters/win32` の境界でだけ許す。`IUnknown**` を要求する API（`DWriteCreateFactory`）は `ComPtr<IUnknown>` で受けて `As()` で問い合わせ、`reinterpret_cast` を書かない。HWND と `this` の対応や `LPARAM` の読み替えは `std::bit_cast`（Issue #3 で実測。clang-tidy が `reinterpret_cast` を一律に拒否する）。

- 機械強制: **planned** → `reinterpret_cast` は clang-tidy `cppcoreguidelines-pro-type-reinterpret-cast`（T5）。`#pragma` は CNF-003。それ以外は CNF の字句検査で拒否する予定

### CPP-010 — 名前が役割を語る

常に禁止する型名の語尾: `Manager` / `Helper` / `Util` / `Utils` / `Common`。
常に禁止するパッケージ／モジュール名: `utils` / `helpers` / `managers` / `misc` / `common`。

承認された役割の語尾は `Port` / `Adapter` / `Query` / `Command` / `Handler` / `Policy` / `Factory` / `Codec` / `Mapper` / `Renderer` / `Reducer` / `Outcome`。
**その型がその役割そのものであるときだけ**使う。

`Processor` や `Data` のように**文脈次第で妥当な語は機械では拒否しない**。機械が拒否してよいのは「常に禁止」だけで、判断が要る語はレビューの仕事である。

- 機械強制: **planned** → CNF-001

### CPP-011 — 1 ファイル 1 主要宣言

ファイルは 1 つの主要な型とその周辺に閉じる。ファイル名は主要型名と一致させる（`TextBuffer.hpp` ↔ `class TextBuffer`）。
名前空間は `nenenib` の下にモジュール名（`core` / `application` / `adapters::win32` / `ui::win32`）。寄せ集めのファイルを作らない。
**実装ファイルの無名名前空間に置く小さな `struct` も数える**（CNF-002 は名前空間の深さを見ない）。ローカルの補助は自由関数と `using` 別名で書く（Issue #3 で実測）。

- 機械強制: **planned** → CNF-002

### CPP-012 — 複雑度に上限を置く

| 指標 | 上限 | 道具 |
| --- | --- | --- |
| 認知的複雑度（関数） | 10 | clang-tidy `readability-function-cognitive-complexity`（T7） |
| 関数の長さ | 60 行 | clang-tidy `readability-function-size.LineThreshold`（T8） |
| ネストの深さ | 3 | clang-tidy `readability-function-size.NestingThreshold` |
| 引数の数 | 4 | clang-tidy `readability-function-size.ParameterThreshold` |
| bool の制御引数（公開 API） | 禁止 | planned（意味と公開境界の検査は未実装） |

Vim のキー列 → 動作のような大きな分岐は `constexpr` の**表**で書く。60 分岐の `switch` は関数長で落ち、同じ表は通る（T8。[ADR 0006](adr/0006-speed-gate-simd-and-table-driven-dispatch.md)）。
閾値を満たすためだけに意味のある処理を割るのは目的に反する。超える必要があるときは**測定可能な理由**を添えて ADR にする。

- 機械強制: **planned** → clang-tidy `readability-*`（.clang-tidy）

---

## 3. 実行時の規律

### CPP-013 — 並行性の形は一つ

スレッドは UI スレッドと、`src/adapters/win32` が所有する**固定のワーカー 1 本**だけ（[ADR 0004](adr/0004-ui-thread-plus-one-worker.md)）。
application はワーカーへ不変の要求値を渡し、完了は UI スレッドの意図として受ける。core / application / ui / app は
`std::thread` / `std::async` / `std::mutex` / `std::atomic` / `CreateThread` / `_beginthreadex` / 同期原始を書かない。共有可変状態を持たない。
flip model の frame latency waitable object を `WaitForSingleObjectEx` で待つのは提示経路の一部であって並行性の導入ではない（スレッドも共有可変状態も作らない。`src/ui/win32` の `Direct2DRenderer::render` だけが行う・ADR 0007）。

- 機械強制: **active** → `eng/symbols.py --require core application` の `concurrency` 分類（`_beginthreadex` / `_Mtx_*` / `_Cnd_*` / `_Thrd_*` / `__imp_CreateThread` 等。TH1）が core / application の実ライブラリで落ちる（2026-09-15・Issue #3）。ui / app はシンボル検査の対象外で、CNF-009 の字句検査だけが見る。`std::atomic` はリンカに見えない（TH2）ので CNF-009 が並行性ヘッダの include を `src/adapters/win32` 以外の `src/` で拒否する

### CPP-014 — 日時・数値・文字集合の扱いを一つに固定する

テキストの内部表現は **UTF-8** に固定する（[ADR 0009](adr/0009-editing-slice-piece-table-and-editing-states.md)。位置はバイトの `Offset`・行の `LineNumber`・code point の `Column` の専用型で、境界でだけ変換する）。Win32 の `W` 系 API と DirectWrite の境界（ui / adapters）
でだけ変換し、`A` 系 API と `setlocale` を呼ばない。日時は使わない（履歴の時刻は adapters が `FILETIME` を受け、core は不透明な値として扱う）。
数値の書式は `std::format` に固定し、`printf` 系を使わない。

- 機械強制: **planned** → `setlocale` / `locale` は ARC-007 のシンボル検査（L1-locale）。`A` 系 API と `printf` 系は CNF の字句検査で拒否する予定

### CPP-015 — 抑制は例外であって道具ではない

抑制には**直前行の `// Waiver: WVR-NNNN`** と有効な waiver 台帳の項目が両方そろっているときにだけ書ける。
ファイル単位・ディレクトリ単位の抑制、静的解析の除外設定、lint の baseline は禁止。

- 機械強制: **planned** → CNF-003 / CNF-004
- 機械強制: **不能**（抑制機能そのものの禁止。`#pragma clang diagnostic ignored` は `-Werror` で指定した診断も抑制できる（M7-suppression-clang-hole）。ADR 0003）

---

## 4. メモリ・Win32・SIMD の規約

### CPP-016 — メモリ安全は検出層で守り、書き方で狭める

C++ はメモリ安全を言語で保証しない。本リポジトリでは次を固定する。

- 可変長配列（VLA）と生の `new` / `delete` を書かない。所有は `std::unique_ptr` / コンテナ / `ComPtr` で、借用は `std::span` / `std::string_view` で表す
- 生配列とポインタ演算を公開 API に出さない。境界で長さを受け取る
- 単体テストと Debug 構成の全 target は AddressSanitizer と UndefinedBehaviorSanitizer 付きでビルドし、`-fno-sanitize-recover=all` で必ず落とす
- 中核は `malloc` / `free` を呼ばない（許可シンボルに無い）

- 機械強制: **planned** → VLA は `-Werror=vla-cxx-extension`（C1-vla-clang。C++ の VLA は clang の拡張として `-Wvla-cxx-extension` が拒否する。`-Werror=vla` は C++ では効かなかったので名指しを変えた・2026-09-15）。所有者の無い `new` は clang-tidy `clang-analyzer-cplusplus.NewDeleteLeaks`、所有権を失った値の再利用は `bugprone-use-after-move`（どちらも `eng/prove-gates.py` で発火を実測）。ASan / UBSan は Debug 構成の全 target に結線する（A1 / A2・`eng/targets.cmake`）。「所有と借用の型」はレビュー事項なので全体としては planned

### CPP-017 — ウィンドウ手続きは意図を渡し、描画は表示値を写す

ウィンドウ手続き（`WndProc`）は操作を application の意図として渡し、描画は application が作った表示値を Direct2D で写す（ARC-011・[ADR 0002](adr/0002-plain-win32-with-direct2d.md)）。
メッセージ番号は開いた OS の集合なので `DefWindowProcW` へ渡す既定分岐を許す。閉じた業務 enum の既定分岐とは区別する（CPP-002）。
HWND・COM インターフェイス・DirectWrite のレイアウトの所有者は 1 つで、`ComPtr` と RAII で閉じる（CPP-016）。
device lost と `DXGI_ERROR_*` は結果型で application へ返す（CPP-005）。GDI で描かない。IME の変換中は Vim の鍵を奪わない（SPECIFICATION）。

- 機械強制: **planned**（`src/ui/win32` から adapters への依存は ARC-002 が拒否。GDI の lib は `platformLibraries` に無いので結べない。意図と表示値の分離はレビュー事項）

### CPP-018 — SIMD は区画に閉じ、関数ごとに機能を宣言する

SIMD の組み込み関数（`<immintrin.h>` 等）は `src/core/simd/` 配下にだけ書き、関数ごとに `[[gnu::target("…")]]` で必要な機能を宣言する。
target 単位の `/arch` は付けない。同じ処理には必ず組み込み関数を使わない実装を置き、どちらを使うかは合成ルートが起動時に決めて注入する
（[ADR 0006](adr/0006-speed-gate-simd-and-table-driven-dispatch.md)）。

- 機械強制: **planned** → target feature の無い関数の組み込み関数はコンパイルエラー（S1-avx2-without-target-clang。`/arch` 無しの target と `[[gnu::target]]` で S2 / S4）。SIMD ヘッダの置き場は CNF-009 の字句検査。fallback の存在はレビュー事項

---

## 5. 依存の方針

新しい依存を足すときは、次の 3 つを揃える。**1 つでも欠けたら足していない。**

1. ADR を 1 本立てる
2. 許可リスト（`eng/architecture.json` の `runtimeDependencies`。ソースを取り込む依存は `platformLibraries` と同じ表に版と SHA-256 を書く）に 1 行足す
3. 下の表に 1 行足す

| 依存 | 用途 | 根拠 |
| --- | --- | --- |
| — | — | 現在の実行時依存は 0。C++ ランタイムは `/MT` で静的に結び、実行ファイルは OS の DLL 以外を import しない（R1・D1） |
| md4c release-0.5.2（予定・未導入） | Markdown プレビュー（FR-007）の構文解析 | [ADR 0003](adr/0003-cpp23-clang-cl-foundation-and-measured-limits.md)。別 target に `/W4 /WX` だけを当て、tag と 3 ファイルの SHA-256 で固定する。導入は FR-007 の Issue で ADR・許可リスト・この表を同時に更新する |

版は lock ファイルで固定し、マニフェストに範囲や `*` を書かない。ゲートは lock が更新される状態を拒む。

---

## 6. 採用しなかった検査と、その理由

**「有効にしなかった」ことも決定である。** 再提案するときは、ここに書かれた理由への反論から始めること。

| 検査 | 不採用の理由 |
| --- | --- |
| 全 lint の一括有効化（`enable-all` / `cppcoreguidelines-*` 群） | 相互に矛盾する規則が同時に入り、規則ではなく道具の機嫌に従うことになる（前例: nene-recall・xi-tools） |
| MSVC `cl` をコンパイラにする | SIMD を無条件に通し（S3）、`auto(x)` が無く（K1-auto-cast-msvc-hole）、`/showIncludes` が日本語（H1）。リンカと SDK にだけ使う（ADR 0003） |
| MSVC `/analyze` を静的解析層に足す | clang-analyzer が同じ null 逆参照を拒否する（T2 と M3-null-analyze）。2 つのコンパイラを走らせると「どちらが正か」が揺れる |
| cppcheck / PVS | 固定した Visual Studio の同梱物ではなく、版の固定先が増える。clang-tidy で足りない検査が実測で出たら再提案 |
| clang-cl に `-Wall` を渡す | `/Wall`＝`-Weverything` と解釈される（Folio K16）。`/W4` を使う |
| `-Wswitch-default`（`default` の強制） | 本規約は網羅済みの `default` を禁じる（CPP-002）。逆向きの規則なので `-Wno-switch-default` で外し、`-Wcovered-switch-default` を使う |
| `-Wpedantic` を外す | COM の `__uuidof` だけが問題（W1）。名指しの `-Wno-language-extension-token` で外し、`-Wpedantic` は残す |
| C++20 モジュール（`import std;`） | CMake ＋ clang-cl ＋ clang-tidy の対応を実測していない。実測して通れば新しい ADR |
| `bugprone-unchecked-optional-access` だけで CPP-004 を守る | MSVC STL の `operator*` を見ない（T3-tidy-unchecked-optional-star-hole）。`value()` の規則と字句検査で補う |
| 正規表現だけで全 API 呼び出しを証明する | マクロ・別名・間接呼び出しを解決しない。`llvm-nm` のシンボル検査を正とし、字句検査は補助にする |

---

## 7. 規約に違反したくなったとき

1. **まず、規約が間違っている可能性を検討する。** 規約は実装より新しくない
2. 規約が正しいなら、設計を変える。抑制で通さない
3. どうしても抑制が要るなら、規則 ID と理由を伴う最小スコープの抑制を書く（CPP-015）
4. **規則そのものを緩めるなら ADR を書く**（QLT-010）。旧規則のどの部分が今も有効かを明記すること
