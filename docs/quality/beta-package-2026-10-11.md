# ベータ梱包の検証記録（#413）

梱包ツールは技術受理。#410はGUI/IMEと性能修正まで受理済み。両PRのCI/main統合と、clean mainからの実生成・起動・公開は後続。

## 対象と理由

配布物の入口・版/依存/ZIP検査だけを変更する。製品動作や性能の再検証はここでは選ばない。正例/反例で、誤ったcommitや版・DLL・同梱物の公開、出力上書きやリンク経由の範囲外出力を捕らえる。製品の検証は#410の対応する記録を再利用する。

## 実行

作業木は`D:/NeNeNib/worktrees/413-beta-package`。原ログは`D:/NeNeNib/outputs/20261011-wrap-beta/commands/package-tool-01`、`package-tool-02`。ツールfixtureは`D:/NeNeNib/outputs/20261011-beta-package/tool-tests-01`、`tool-tests-02`。

```text
python -B eng/test-package-release.py --work-dir D:/NeNeNib/outputs/20261011-beta-package/tool-tests-01
python -B eng/test-package-release.py --work-dir D:/NeNeNib/outputs/20261011-beta-package/tool-tests-02
```

初回は8 testsのうちジャンクション拒否が失敗。`stat`がリンク先の属性を返すため、親の`lstat`を調べるよう修正した。失敗ログとfixtureを保存し、修正後の2回目は8 tests全て成功（1.863秒、exit 0）。CMake版の一意性、出力範囲/既存出力、実ジャンクション、production依存グラフ、x64/埋め込み版/DLL、ZIPの3項目/bytes、clean/main/remoteの状態、正のbeta番号を対象とする。

関連する実装・検証入力が不変ならこの結果をpush/review/mergeで再利用する。実exeの検査、clean mainからのZIP実生成、展開物の短い起動、公開後の再取得照合は別に記録するまで未測。全件検証・基準値変更・ゲート緩和は行っていない。Waivers: none。

`package-inspection-01`（6.114秒、exit 0）は#410の実Release 49f3393のPEを梱包検査器へ通し、x64/数値版/宣言済みWindows DLL 12個/NotSignedを確認。exeを起動していない。実際のdirty treeを指定したCLI呼び出しも、build/出力を作る前に拒否した。生データは`D:/NeNeNib/outputs/20261011-beta-package/real-executable-inspection.json`、`real-pe-report.txt`、`dirty-refusal.json`。署名無しを署名済みとは扱わない。

`package-tool-03`はmainと同じcommitのD側detached作業木を許す境界を追加して8 tests成功（2.045秒）。`package-tool-04`は外部のビルド/PE検査の境界をfixtureへ置いた全梱包フローの正例を追加し、9 tests成功（2.691秒）。生成したZIP名・展開3点のbytes・READMEの版置換・SHA256SUMS・package.jsonと、公開/起動を行わないことを照合した。fixture exeを実製品とは扱わず、実製品のZIP生成はmain統合後に実施する。原記録は同commands配下、fixtureはtool-tests-03/04。

自己レビューで、CMake唯一の数値版、既存Release入口だけの呼出し、production依存グラフ、clean/remote-main/新規出力/リンクの拒否、完全な3項目/bytes照合、公開操作を持たないことを確認。製品コード/保存schema/性能基準は変更していない。関連入力が不変の結果をPR/CI/mainでも再利用する。
