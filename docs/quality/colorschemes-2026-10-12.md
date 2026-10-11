# 配色拡充の検証 — 2026-10-12

Issue #419、FR-016、ADR 0117。規則: ARC-001/002/003/004/007/009/011、CPP-002/003/004/005/011/012/014/016、QLT-001/002/004/008/009/011/012/013、GIT-001/002/003/004。Waivers: none。

## 対象と変更

- `BuiltinTheme`/`builtin_themes`を9件から95件へ拡充。Vim標準28、VS Code標準16、その他IDE34、MonaLisa、独自映画7を追加し、16構文役割・出典を埋めた。正確な値/版/調整は[設計](../design/2026-10-12-colorschemes.md)と[調査記録](../theme-sources.json)。
- `Palette.gutter_background`、整数`derive_ui`の互換既定と`with_gutter`、v1利用者テーマの任意キー。省略時は上書き後の本文背景へ戻し、新規必須キーはない。
- `Direct2DRenderer`は欄の背景を描き、現在行の帯を本文へ限定。現在行番号は本文前景、通常番号はgutter。寸法、折り返し、ヒットテストの実装は変更しない。
- `CommandChoice`の共通候補生成でASCII大小文字を無視したテーマ名の完全一致を優先。新しい`red`が`delek`と部分一致の同点になった初回失敗を修正。Exの前方一致は維持。
- `THIRD_PARTY_NOTICES.txt`と配布ZIPの4つ目のエントリ、形式説明、仕様、README、静的HTML見本。settings.v3/session.v2/theme v1の版は不変。

## 範囲と結果

調査・実装・検証はサナ単体。全件Vim、編集/undo等の無関係なscope、性能計測、full checkは実施していない。初回Debug/Releaseは必要ターゲットを2並列でビルドし、修正後は差分だけをコンパイルした。

作業証跡の一時位置は `D:/NeNeNib/outputs/20261012-colorschemes`、恒久収載先は `D:/NeNeNib/evidence/colorschemes-20261012`。以下のラベルはその`commands/<label>/{record.json,stdout.txt,stderr.txt}`を指す。整理後は恒久先`files/outputs/20261012-colorschemes/`から読む。

| 検証コマンド/対象 | 退行リスク・結果 | 証跡 |
| --- | --- | --- |
| `cmake --build build --target nib_tests nib_theme_tests --parallel 2` | 新しい共通型/表の利用箇所、静的解析、ASan/UBSan。成功。初回は162 build steps | build-debug |
| `cmake --build build-release --target NeNeNib --parallel 2` | 製品レンダラーを含むReleaseのコンパイル。成功。候補順位修正後は3 stepsだけ差分再ビルド | build-release, finish-builds |
| `build/nib_tests.exe --themes` | 95名称の独立した期待一覧、全役割/出典、本文/UI/行番号の比、追加86種の構文10役割、旧採用色/導出規則。4235 checks成功 | test-themes-fixed |
| `build/nib_tests.exe --ex-settings` | テーマ選択・設定保存/復元・全候補と前方一致。483 checks成功 | test-ex-settings-fixed |
| `build/nib_tests.exe --command-palette` | 共通候補の検索と実行、95種完全一致と短名RED。583 checks成功 | test-command-palette-fixed |
| `build/nib_tests.exe --user-theme-values` | 組み込み名予約の拡張と所有値。21 checks成功、表変更後の結果を再利用 | test-user-theme-values |
| `build/nib_tests.exe --user-theme-selection` | 組み込み増加後の利用者候補の順/選択と失敗表示。97 checks成功 | test-user-theme-selection-fixed |
| `build/nib_theme_tests.exe` | 任意キーの正例、重複/不正色、省略時の本文背景追従、全組み込み/利用者テーマのcodecと実ファイル境界。514 checks成功 | test-theme-codec |
| `python eng/test-package-release.py --work-dir <D側専用dir>` | 新しい出典エントリを落とさず梱包・展開照合する境界。9 tests成功 | test-package-fixed |
| `clang-format --dry-run --Werror <変更C++17ファイル>` | 変更表/型/描画/候補/試験の整形。成功 | finish-builds |
| `python eng/symbols.py --build-dir build --require core application` | coreの大きな表と候補関数からOS依存を持ち込まない。2 libraries、0 violations | finish-builds |
| canonical `source_checks`/`document_checks`/`architecture_checks`を変更ファイルへ適用 | C++の規則、新ヘッダーの所有/依存、CMake File APIの実ターゲット境界。0 violations、active waiverなし | scoped-static |

全件再実行はしない。初回`--ex-settings`はdで始まる名前の増加に対する古い期待、`--command-palette`はredの同点順、梱包試験は新しい出典ファイルを持たないfixtureで失敗した。初回ログを保持し、原因を修正して該当scopeだけ再実行した。`--themes`は構文比の追加検査が変わったため再実行。codec/利用者値は以降の候補採点変更に依存しないため成功結果を再利用する。

## 実画面と見本

hideの03:20〜03:25の了承内、03:20〜03:23:27 JSTに専用プロファイルと専用`palette-check.py`で確認し、2つの確認用プロセスは終了した。利用者が開いているbeta.2の窓は操作しない。

実描画は`build-release/NeNeNib.exe`、SHA256 `66338b4514a7133188baf806138ff4043e485a9847d5e4005e2e5797d2141d05`。Computer Useのキー/ドラッグ入力で切り替え、既存`window_driver.capture_png`で隠されていないclientを保存した。

- alloy-core / relic-sand / monalisaで折り返した現在行、通常番号と現在番号、暗い/明るい欄を目視した。
- monalisa / code-rainで空の文書を確認。現在行の帯が行番号欄を上書きしない。
- code-rainを選んで閉じ、同じ専用settings.v3/session.v2で再起動して配色/文書を復元。
- 幅800→588物理pxへ縮小し、折り返しの増加後も番号が重複せず欄を保つ。
- 7 PNGすべてで、欄内の同一x座標の330画素がテーマの`gutter_background`と完全一致。現在行色の画素が本文内に存在することも確認。`gui/result.json`に各画像の寸法/画素数を保存。

`docs/design/colorschemes.html`はC++色表から抽出した95件の見本。Edgeの独立したheadlessプロファイルで1440×1300に描画し、検索/分類/明暗のコントロール、Python/TypeScriptの見本、映画7種とMonaLisaのカードのレイアウトを目視した。製品のシンタックスハイライトが動くという証拠には使わない。

## 再利用と残る制限

push/レビュー/merge/文書追記で製品と試験の関連入力が変わらない限り、この結果を再利用する。配布物の正規ビルドとZIP照合は成果物の同一性を記録する工程で、単体/実画面試験を繰り返さない。

Python/TypeScriptの言語解析は未実装。構文色の準備まで。Vimは調査時の28標準名と各テーマの代表GUI配色を採用し、全cterm/明暗変種は網羅しない。新しい予約名と既存利用者テーマの衝突では従来の予約名エラーを返し、ファイルは変更しない。公開済みbeta.2の内容はこの変更で更新しない。

正規梱包・統合・恒久収載/整理の最終結果はこの記録への追記と恒久先`closeout.json`で記録する。Issue #408の別機能を開始しない。

## ローカル確認用ZIP

`pwsh -NoProfile -File eng/package-release.ps1 -Beta 3 -Preview -OutputDirectory D:/NeNeNib/releases/419-colorschemes-preview -Verification docs/quality/colorschemes-2026-10-12.md` をcleanな製品commit `ad23bd89727da336b71b3a318204ebe3f7791258` で実行。365.170秒、exit 0。正規Release経路、x64/埋め込み版0.1.0/12宣言Windows DLL、直下4エントリと展開bytesの一致を確認した。

ZIP: `NeNeNib-0.1.0-beta.3-windows-x64.zip`、630542 bytes、SHA256 `0a397c25d14b688e76fa5b6d919546db721d20083bf17dd2243c55861e8f2c8c`。同じディレクトリの`package.json`/`SHA256SUMS`/`pe-inspection.txt`が正規記録。`NeNeNib.exe`、`README.txt`、`LICENSE`、`THIRD_PARTY_NOTICES.txt`を含む。未署名。preview=true、公開していない。

梱包後のexeは起動していない。実画面確認は上記の同じ製品ソースで作ったReleaseの結果を再利用する。梱包後の変更は状態文書・リンク・記録だけで、製品/試験/依存/検証環境の変更はない。公開済みbeta.2のtag/assetは変更しない。

自己レビューで単一色表、型の依存方向、利用者テーマ省略互換性、現在行と欄の描画順、出典保持、限定検証と再利用を照合。後続の文書だけはcanonical `conformance.document_checks`と`git diff --cached --check`を確認する。通常PR/必須check/正規merge後のmain SHA、恒久収載の全SHA照合、作業木/出力の監査整理の実結果は`D:/NeNeNib/evidence/colorschemes-20261012/closeout.json`へ記録する。
