# UTF-8保存を本文断片へ接続する契約確認

2026-10-12 / Issue #442 / ADR 0119。段階1Aの保存の土台であり、2GiB成立性試験の実施記録ではない。

## 変更と検証範囲

TextBufferのlvalueから借用した断片を、FilePortの唯一の仮想書込へ渡す。UTF-8 BOMは先頭の静的断片、Shift_JISは既存の検証付き変換で作った1断片となる。Win32FileAdapterは全断片の後に1回Flushし、従来の一時ファイル/置換/create_newを使う。変更した7製品ファイルと4テストファイルを整形検査し、FilePortを使う設定/履歴等も同じビルドで型を確認した。

| 確認する退行 | 対象・コマンド | 結果 |
| --- | --- | --- |
| 借用の本文順、範囲、編集分岐後のprefix、元本文の共有 | `build/nib_tests.exe --buffer-range` | 289 checks、成功 |
| BOM/混在改行/空本文、通常/Ex保存、undo/redoと保存済み位置、変換/書込拒否 | `build/nib_tests.exe --ex-document`（file-save契約を含む） | 218 checks、成功 |
| Exの名前付き保存、create_new、文書名維持/更新と失敗時の保持 | `build/nib_tests.exe --ex-write-path` | 90 checks、成功 |
| 実ファイルの断片/NUL/空列、create_new衝突、ロック中の置換拒否と元ファイル/一時ファイル保持 | `build/nib_adapter_tests.exe --files`（cwd: build） | 99 checks、成功。Win32/ファイルシステム依存 |
| port変更と直接の呼出し元の型、警告、静的解析、製品リンク | `cmake --build build --target nib_tests nib_adapter_tests NeNeNib --parallel 4` | Debug、clang-cl/clang-tidy、ASan/UBSan、成功 |
| 中核がOS/ファイル/スレッドへ依存しない | `python eng/symbols.py --build-dir build --require core application` | 2 libraries、違反0 |
| 変更したC++の整形 | `clang-format --dry-run --Werror <変更した各cpp/hpp>` | 11ファイル成功 |

規約検査は`python eng/conformance.py --build-dir build`、空白は`git diff --check`で確認する。検証の元入力SHAと最終結果はPRの記録から参照する。

実行環境はWindows 11、プロジェクト固定のclang-cl 19.1.5/MSVC linker/SDK、D:/NeNeNibの隔離worktreeとbuild。実ファイルの生成/削除はテスト所有の`build/nib-adapter-files`だけ。実ユーザー文書・設定・クリップボード・通常試用の窓を使っていない。全件テスト・GUI・性能測定は実施していない。

## 証拠と再利用

恒久記録は`D:/NeNeNib/evidence/chunked-save-442/`。`configure.log`、初回の失敗も含む`build.log`、修正後の`build-fixed.log`、`contracts.log`、実行したexeの`binaries.json`、検証した11ファイルの`verified-sources.json`を保持する。

初回ビルドは、追加したテスト補助の外部可視性（missing-prototypes）とwrite_documentの分岐の深さ（4 > 3）で失敗した。補助を無名名前空間に閉じ、UTF-8断片の組み立てをprivate関数へ分けて修正した。警告・閾値・除外を変更せず、修正後のビルドと上表の契約が成功した。成功まで同じ試験を繰り返した事例ではなく、具体的なソース修正に対応する検証である。

成功後は実装・テスト・関連依存が不変なら、文書追記・mainの文書変更の取込み・commit SHA・push/レビュー/mergeを理由に再実行しない。新しい実装差分がある場合だけ関連対象を選び直す。

## 自己レビューと限界

- ARC-001/003/004/005/007/009/010/012: 本文の所有者・編集経路は維持、借用は同期call内だけ。旧文字列の書込も1断片へ包んで同じ実装へ入る。文字コード/改行/保存済み位置の契約と、adapterだけがファイルに触る境界を維持する。
- CPP-002/004/011/012/013/014: 文字コードの分岐を網羅し、optionalの裸の読取・新しい型の重複・スレッド・OS文字コード変換経路を増やさない。QLT-001/008/010/012/013: 保存と借用の直接の境界だけを検証し、速度や2GiBの証拠に広げない。Waivers: none。
- 読込上限64MiB、originalの所有文字列と全文改行索引、同期I/Oは現状どおり。大容量試作A/Bは未完了。Shift_JISは全文変換の一時領域を引き続き使う。
- 部分書込/Flushの故障注入と電源断耐性は未確認。既存の失敗返却/一時ファイル処理は維持し、今回の実試験は置換拒否を確認した範囲まで。
- 断片が非常に多い本文はWriteFileの回数が増える。メモリ/速度の定量効果や「退行なし」は主張しない。ワーカーへ借用列だけを持ち出してはいけない。
