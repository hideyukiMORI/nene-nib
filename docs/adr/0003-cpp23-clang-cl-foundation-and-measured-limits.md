# ADR 0003 — C++23 / clang-cl の検査基盤と実測できた限界を固定する

- 状態: 受理
- 日付: 2026-09-15
- Issue: #1
- 影響する規則: ARC-002 / ARC-003 / ARC-007 / CPP-002 / CPP-003 / CPP-004 / CPP-005 / CPP-012 / CPP-013 / CPP-015 / CPP-016 / CPP-018 / QLT-001 / QLT-007 / QLT-011 / CNF-001 / CNF-006 / CNF-009

## 文脈

Phase 0 の実測（MSVC 19.44.35228 / LLVM 19.1.5 / Windows SDK 10.0.26100 / 114 記録）は `eng/measure-language.ps1` と
`eng/probes/language.json` から再現し、記録は `docs/quality/phase0-results.json`。Loupe の 19 ケース（M1〜M8・MSVC）を
再現したうえで、clang-cl を同じケースに掛け、C++23 の機能・SIMD・並行性・DirectX・md4c・Vim を Nib のために足した。要点は 6 つ。

1. **MSVC の STL が持つ C++23 の機能は clang-cl でも全部通る**（K1-*: `std::expected`・deducing this・`if consteval`・
   static `operator()`・多次元 `operator[]`・`std::print`・`ranges::fold_left`・`to_underlying` / `unreachable`・`uz`・
   constexpr `unique_ptr`・consteval な書式検査）。逆に **cl は `auto(x)` を通さない**（K1-auto-cast-msvc-hole・C3537）
2. **SIMD の許可範囲を機械で強制できるのは clang-cl だけ。** `/arch` も target 属性も無い関数で `_mm256_*` を書くと
   clang-cl は `requires target feature 'avx'` で落ち（S1）、`/arch:AVX2`（S2）か `[[gnu::target("avx2")]]`（S4）で通る。
   cl は何も無くても通す（S3-avx2-without-arch-msvc-hole）
3. **決定性・依存・並行性はリンカ段で見える。** `system_clock::now()` は `_Xtime_get_ticks`、`steady_clock` は
   `_Query_perf_counter`、`random_device` は `?_Random_device@std@@YAIXZ`、`std::filesystem` は `__std_fs_*`、
   `std::locale` は `?_Init@locale@std@@…`、Win32 は `__imp_GetTickCount` / `__imp_CreateFileW`（L1 / L2）。
   `std::thread` は `_beginthreadex`、`std::mutex` は `_Mtx_*`、`condition_variable` は `_Cnd_*`、`CreateThread` は
   `__imp_CreateThread`、`std::async` は `Concurrency` 名前空間の ConcRT 群を引き込む（TH1）。
   **ただし `std::atomic` はインライン化されてシンボルを残さない**（TH2）。共有可変状態はリンカでは見えない
4. **道具の癖。** COM の `__uuidof` / `IID_PPV_ARGS` は `-Wpedantic` の下で `-Wlanguage-extension-token` として落ちるので、
   名指しの `-Wno-language-extension-token` が要る（W1）。clang-cl の `/showIncludes` 接頭辞は VSLANG に関係なく ASCII の
   `Note: including file: ` で、cl は日本語の `メモ: インクルード ファイル:  ` を出す（H1。Loupe Issue #5 の CP932 問題は
   clang-cl では起きない）。CMake 3.31 は clang-cl の `CMAKE_CXX_STANDARD 23` を `-clang:-std=c++23` に写す（CM1。
   C23 では知らなかった Folio と違い、C++23 は標準の変数で書ける）
5. **DirectX は静的リンクの exe から窓なしで呼べる。** WARP の D3D11 device・Direct2D 1.3 factory と device context・
   DirectWrite 3 factory・DXGI 1.6 factory・composition 用の **flip model（`FLIP_SEQUENTIAL`）＋ waitable swap chain**・
   DirectComposition device・`DwmGetColorizationColor` を順に呼び、描画して Present するまで終了 0。exe の import は
   `d2d1` `DWrite` `dxgi` `d3d11` `dcomp` `dwmapi` `KERNEL32` だけで、CRT の DLL は無い（D1-directx-static-clang-cl・
   D1-directx-static-cl）。Mica（`DWMWA_SYSTEMBACKDROP_TYPE`）は HWND が要るので、定数の存在だけを確認した
6. **md4c（release-0.5.2・commit 729e6b8）は clang-cl の `/W4 /WX` では警告ゼロ**、Nib の厳格集合では `-Wcast-qual` と
   `-Wsign-conversion` で 20 件を超えて落ち、cl の `/W4 /WX` でも C4127 / C4200 / C4244 で落ちる（MD1-*）。
   厳格集合は「Nib が書く C++」の規約であって、他人の C に当てるものではない

言語で塞げない穴も実測した: 範囲外の enum 値（M1-*-out-of-range-hole）、公開 aggregate（M2-public・T1-tidy-aggregate-hole）、
`const_cast`（M2-const-cast-clang-hole。C スタイルキャストは `-Wold-style-cast` で落ちる）、`mutable`（M2-mutable-hole）、
`nullptr` の逆参照（M3-null-clang-hole。clang-tidy が拒否＝T2）、**空の `std::optional` を `operator*` で読む**
（M3-optional-hole・T3-tidy-unchecked-optional-star-hole。`value()` なら clang-tidy が拒否＝T3-tidy-unchecked-optional-value）、
値初期化の `Id{}`（M3-value-init-hole）、`malloc`＋`static_cast` の偽造（M4-forge-hole）、`#pragma clang diagnostic ignored`
（M7-suppression-clang-hole。`-Wswitch` と `-Wswitch-enum` の両方を黙らせれば通る）。

## 決定

**clang-cl（LLVM 19.1.5、Visual Studio 同梱）を唯一の C++ コンパイラとし、MSVC の toolset はリンカ・SDK・`rc` のためだけに入れる。
製品・測定ビルド・将来の md4c まで同じ 1 本で、cl は Phase 0 の比較対象としてだけ残す。**

- 言語は C++23（`CMAKE_CXX_STANDARD 23`。CMake が `-clang:-std=c++23` を渡す＝CM1）。clang-tidy の cl ドライバが `/clang:` 引数を
  落とす穴（Folio ADR 0003）を疑い、`--extra-arg=/clang:-std=c++23` でも同じ標準を渡す。`tests/build/ToolchainSmoke.cpp` は
  C++23 専用構文だけで書き、tidy が C++17 で動いた瞬間に落ちる
- 警告集合は `eng/targets.cmake` の 1 か所（`/W4 /WX /utf-8 /EHsc` ＋ `eng/probes/language.json` の `clangStrict` と同じ並び）。
  `-Wswitch-enum` / `-Wcovered-switch-default`（CPP-002）、`-Wold-style-cast` / `-Wcast-qual`（CPP-003）、
  `-Wzero-as-null-pointer-constant`（CPP-004）、`-Wunused-result`（CPP-005）、`-Wconversion` / `-Wsign-conversion` /
  `-Wshadow` / `-Wimplicit-fallthrough` / `-Wmissing-prototypes`（C1〜C7）、`-Werror=vla-cxx-extension`（CPP-016。C++ の VLA は
  `-Wvla-cxx-extension` が拒否する。Folio から写した `-Werror=vla` は C++ では効かず、Phase 2 の反例で判明したので名指しを変えた）。
  `-Wno-switch-default` は CPP-002 と逆向きの規則を外すため、`-Wno-language-extension-token` は COM のために外す名指しの選択
- 整形の正本は `.clang-format`、lint の正本は `.clang-tidy`。名指しで入れるのは、null 逆参照・未初期化・`new`/`delete` の解析器、
  `bugprone-unchecked-optional-access`（T3）、`bugprone-use-after-move`、`cppcoreguidelines-pro-type-const-cast`（T4）、
  `-reinterpret-cast`（T5）、`-cstyle-cast`、`cppcoreguidelines-avoid-non-const-global-variables`（T6）、
  `misc-non-private-member-variables-in-classes`（T1）、`readability-function-cognitive-complexity` 10（T7）、
  `readability-function-size` 60 行・ネスト 3・引数 4（T8）。一括の lint 群は採用しない。発火を証明できないものは外す
- モジュールの許可グラフは `eng/architecture.json`。実ターゲットは具体的な責務とソースがある時だけ作る。今回作る C++ ターゲットは
  C++23 基盤のスモークテストだけで、製品モジュールは作らない。OS ライブラリの許可表には ADR 0002 が決めた DirectX の集合を最初から書く
- 決定性・依存方向・並行性の正は `eng/symbols.py`（`llvm-nm --undefined-only` ＋ `eng/symbol-allowlist.json`）。分類は 3 つ:
  非決定入力（ARC-007）・並行性の原始（CPP-013）・それ以外の宣言外シンボル（ARC-003）。`eng/conformance.py` の字句検査は補助で、
  リンカに見えない `std::atomic` と SIMD ヘッダの置き場だけを CNF-009 が字句で見る
- 中核の許可シンボルは L3-stl-baseline-undefined の 14 個（`operator new` / `delete`・`_Xlength_error`・例外の枠組み・
  `memcpy` 系・security cookie）とサニタイザの記号から出発する。`malloc` / `free` は許可しない（C++ の中核は `new` を
  所有者経由でだけ使う。M4-forge-hole を狭める）。`__std_fs_*`・`locale`・`__std_system_error_*` も許可しない
- 単体テストは ASan / UBSan 付きでビルドし `-fno-sanitize-recover=all` で必ず落とす（A1 / A2）。CRT は常に `/MT`（R1・Debug でも
  `-MTd` にしない）。中核の分岐カバレッジ目標は 90%（V1 で clang-cl の C++ でも `llvm-cov` が動くことを実測）だが、
  測る対象がまだ無いので QLT-009 は planned
- **md4c は Issue #1 では入れない。** 入れるときは `third_party/md4c` の別 target に `/W4 /WX /utf-8`（C17）だけを当て、
  Nib の厳格集合とサニタイザ以外は当てず、ソースを書き換えない。固定は tag `release-0.5.2` / commit `729e6b8b320caa96328968ab27d7db2235e4fb47` と
  3 ファイルの SHA-256（`src/md4c.c` `CFB0B3D7…2DE72`・`src/md4c.h` `346C3A1D…0300F`・`LICENSE.md` `99C75206…37102`）を
  規約検査が照合する形で行う。導入は Markdown プレビュー（FR-007）の Issue で、ADR・許可リスト・規約の表の 3 点をそろえる

## 強制

規則ごとの正本は `docs/QUALITY_GATES.md`。自作検査は正例・反例の自動テスト（`tests/conformance`）をゲートに結び、
実ツールの発火は `eng/prove-gates.py` が毎回確かめ、証拠を `docs/quality/gate-proofs.md` に残す。CI / ruleset は実際に反映・確認するまで planned。

## 結果

- Windows の道具と既定ロケール・環境に触れる検証スクリプトは開発時の道具であり、製品の ARC-007 の区画には含めない。検査器自身は
  日付を引数でも受け、期限のテストを固定できる
- 検査スクリプトの依存は Python / PowerShell 標準機能のみ。実行時依存は 0
- 緑は検査基盤の正常性を示し、Nib が動作することは示さない
- 🔴 メモリ安全・範囲外 enum・`const_cast` / `mutable`・空の optional の `operator*`・`malloc` 偽造・pragma 抑制・共有可変状態
  （`std::atomic`）は言語でもリンカでも塞げない。対応する規則は「検出層で守る」「書き方で狭める」と書き、塞げたとは書かない
- 施主の当初案「C++23 / Win32 は Loupe 系統」のうちコンパイラだけを変える。Loupe は製品を cl、測定ビルドを clang-cl で作って
  いたので、実は 2 本だった。Nib は 1 本にする

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| MSVC `cl` を C++ のコンパイラにする（Loupe と同じ） | SIMD を無条件に通す（S3）、`auto(x)` が無い（K1-auto-cast-msvc-hole）、`/showIncludes` が日本語（H1・Loupe Issue #5）、UBSan が無い、`-Wold-style-cast` / `-Wcovered-switch-default` 相当が無い。cl が拒否して clang-cl が通す穴は 1 つも実測されなかった |
| `cl` と `clang-cl` の二本立て（製品は cl、測定と lint は clang-cl） | 「どちらのコンパイルが正か」が揺れる。Loupe はこの形で、カバレッジの exe と製品の exe が別のコンパイラだった |
| clang-cl に `-Wall` を渡す | clang-cl は `-Wall` を `/Wall`＝`-Weverything` と解釈する（Folio K16 で実測。Nib では再測していない）。`/W4` を使い、群は入れない |
| `-Wswitch-default` を残す | 網羅済みの `default` を禁じる CPP-002 と逆向き。`-Wno-switch-default` で外し、`-Wcovered-switch-default` を使う |
| `-Wpedantic` を外して COM を通す | 外すと `-Wlanguage-extension-token` 以外の警告も消える。名指しの `-Wno-language-extension-token` だけを外す（W1） |
| 全 lint の有効化 | 規則ごとの責務と採用理由が失われる |
| cppcheck / PVS 等の外部静的解析器 | 固定した Visual Studio の同梱物ではなく、版の固定先が増える。clang-tidy で足りない検査が実測で出たら再提案 |
| C++20 モジュール（`import std;`） | CMake ＋ clang-cl ＋ clang-tidy のモジュール対応を実測していない。未実測のものをゲートの土台にしない。実測して通れば新しい ADR |
| md4c を Nib の厳格集合でコンパイルする | 20 件超の `-Wcast-qual` / `-Wsign-conversion` で落ちる（MD1）。他人のコードに自分の規約を当てて書き換えると、版を上げるたびに差分を抱える |
| md4c を Issue #1 で取り込む | 製品モジュールが無いのに依存だけ先に入れると、「具体的にいま必要」の条件（DEVELOPMENT_WORKFLOW 第 6 節）を満たさない |
| `dumpbin` をソース上の API 名で grep する | `time()` → `_time64`、`system_clock::now()` → `_Xtime_get_ticks` の写像で空振りする。`llvm-nm` のシンボル名を許可リストと完全一致で照合する（Folio ADR 0003 と同じ） |
| 未実装の層を空のライブラリで作る | 具体的な責務がない将来用モジュールになる。`symbols.py` の「0 個で通る」を active と偽ることになる |
| カバレッジ 90% を active と記録する | 中核の分岐も測定対象もまだ存在しない |

## 参考

- [clang diagnostic flags](https://clang.llvm.org/docs/DiagnosticsReference.html)
- [clang-tidy function-size](https://clang.llvm.org/extra/clang-tidy/checks/readability/function-size.html)
- [clang-tidy unchecked-optional-access](https://clang.llvm.org/extra/clang-tidy/checks/bugprone/unchecked-optional-access.html)
- [MSVC STL の C++23 対応状況](https://github.com/microsoft/STL/wiki/Changelog)
- NeNe Loupe ADR 0003（C++ の実測。19 ケース・MSVC）、NeNe Folio ADR 0003（C23 の実測。clang-cl・リンカ段の検査）
