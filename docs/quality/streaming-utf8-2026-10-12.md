# 分割UTF-8検証の準備記録

2026-10-12 / Issue #452 / [ADR 0122](../adr/0122-utf8-validation-retains-only-the-unfinished-sequence.md)。

## 現在の状態

**試験はhide指示で保留。実装と契約試験コードの準備段階で、技術受理していない。** Debug/Release build、clang-tidy、ASan/UBSan、単体試験、symbols、conformance、実機性能、CIは未実行。成功件数や性能値はまだない。draftのままmainへ統合しない。

## 変更と確認対象

- `Utf8Validation.hpp`に不変の進捗を追加。`empty` / `with_bytes` / `finish`で断片とEOFを区別し、最大3 bytesの未完列だけを所有する。
- `Utf8.cpp`の既存走査を分割に対応させ、一括`validate_utf8`も同じ走査へ接続。文字集合の表・復号を二重化せず、従来のASCII連続走査を保つ。累積byte数は加算前にoverflowを検査する。
- `Utf8ValidationTests.cpp`に既知の文字列の全2/3分割、拒否、EOF、借用元の変更、進捗の分岐、未完列の後の別文字を追加。`Scopes.hpp` / `CoreTests.cpp` / `CMakeLists.txt`から既存`--utf8`と既定実行へ接続する。
- ADR索引とcurrentへ試験保留中であることを記録。FilePort、DetectedTextの所有契約、保存schema、64MiB上限は変更しない。

## 再開後に必要な最小の確認

| 対象 | 理由・予定する確認 |
| --- | --- |
| 変更target | 新しい型/署名と実装をcompileし、既存のDebug/tidy/ASan/UBSan条件で拒否がないか確認する |
| `build/nib_tests.exe --utf8` | 新しい分割契約と、一括検証・表示文字列・UTF-16という直接の呼出し境界 |
| `build/nib_tests.exe --file-text` | BOM/文字コードの判定、所有本文、controllerの読込を通じた不正入力の拒否 |
| `eng/symbols.py` / `eng/conformance.py` | coreへOS/CPU/並行性の依存を持ち込まず、型・可視性・正典のtargetへの接続を保つ |
| 直接のUTF-8/読込の性能 | 一括の走査も変わるため必要。既存のprobe/ベンチと固定した比較条件で実施し、#446とは別の対象版として記録する |

試験の保留が解除されてから対象版・入力・回数と実機の時間帯を固定する。無関係なVim全件や同じ検証の工程ごとの繰り返しは行わない。旧validatorの成功結果を変更版の成功として再利用しない。

## 実行した差分確認

`eng/toolchain.ps1`で固定したclang-format 19.1.5を使い、変更C++6ファイル（Utf8.cpp/.hpp、Utf8Validation.hpp、Utf8ValidationTests.cpp、CoreTests.cpp、Scopes.hpp）を整形した。`clang-format --dry-run --Werror <同6ファイル>`と`git diff --check`は成功。これは構文・型・静的解析・動作の成功を意味しない。コミット時の空白確認も同じ差分を対象に行う。

## 自己レビューと限界

ARC-001・CPP-014に従って、UTF-8の意味を既存`Utf8.cpp`へ集約する。ARC-003/007・CPP-013/018の区画は変えない。ARC-005/008・CPP-003/007では進捗の不変性と未完列の所有だけを保証対象にし、本文を所有しない進捗を検証済み本文の代用にしない。ARC-010・CPP-004では未完・拒否・長すぎる入力を結果と明示的なEOFで扱う。

保持するcode point数は受理したbyte数以下。`incoming > max - received`のguardで加算overflowを防ぐ設計だが、`size_t`上限まで積む実行確認はしていない。試験専用の内部状態setterは追加しない。性能影響、コンパイルと静的解析の成立、2GiBでの成立はいずれも未確認。

適用規則はADRに列挙したFR-008/013、ARC/CPPとQLT-001/008/012/013/014。Waivers: none。

## 保存先

作業木は`D:/NeNeNib/worktrees/452-streaming-utf8`、準備出力は`D:/NeNeNib/outputs/streaming-utf8-20261012`。未検証・未統合のため保持し、受理後に恒久収載と安全確認を行って整理する。mainと公開版は変更しない。
