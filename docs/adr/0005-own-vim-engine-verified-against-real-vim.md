# ADR 0005 — Vim は自前実装で、本物の Vim との差分テストで再現度を担保する

- 状態: 受理
- 日付: 2026-09-15
- Issue: #1
- 影響する規則: ARC-001 / ARC-003 / ARC-004 / CPP-012 / QLT-008 / QLT-013

## 文脈

施主決定 D3（Vim は自前実装。Neovim を載せない・Vim のコードは写さない）と D4（本物の Vim との差分テストで機械的に担保する）が与えられている。
再現度の目標は T1＝ヴァニラの Vim の既定動作、T2＝VSCodeVim で困らない水準（SPECIFICATION 第 4 節）。ライセンスは MIT を維持する。

Phase 0 で oracle の実現性を実測した（[phase0-results.json](../quality/phase0-results.json) V2-vim-headless-oracle）:
この端末の Vim 9.1（2024-01-02）を `-u NONE -i NONE -N -n -es -S probe.vim input.txt` で起動し、`normal! ggdwjp` を流して
本文・カーソル・無名レジスタをファイルへ書かせると、終了コード 0 で `beta` / `galpha amma` / `cursor=2,7` / `reg=alpha ` が
得られ、2 回の実行で完全に一致した。設定ファイルもプラグインも読まないので、結果は Vim の版だけに依存する。

## 決定

**Vim のエンジン（バッファ・カーソル・レジスタ・モード・オペレータ・テキストオブジェクト・`.` とマクロ）は `src/core` の純関数として
自前で書く。再現度は「同じ初期テキストとキー列を自前エンジンと headless の本物の Vim に流し、本文・カーソル・レジスタを突き合わせる」
差分テストで担保する。Vim のソースコードは読まず、振る舞いだけを oracle にする。**

- oracle は開発時の道具（`eng/` のスクリプト）であり、製品にも単体テストにも Vim を要求しない。oracle が生成した期待値は
  fixture（初期テキスト・キー列・期待する本文／カーソル／レジスタ）としてリポジトリに保存し、単体テストは fixture を再生する。
  fixture の再生成だけが Vim を要る
- Vim の版は `eng/tool-versions.json` に書く（fixture の版が変われば期待値の差分として現れる）
- キー列 → 動作の対応は表駆動（[ADR 0006](0006-speed-gate-simd-and-table-driven-dispatch.md)）。`normal!` で流せるキー列を
  そのまま fixture の入力にする
- T1 の範囲外（`.vimrc`・Vim script・プラグイン）は fixture を作らない。将来 `map` / `set` を足すときは fixture の形を変えずに
  「設定」を入力の一部にする

## 強制

- **active**（Issue #22・ADR 0012）: fixture の再生は単体テスト `nib_unit` が `tests/vim/VimFixtures.hpp`（`eng/vim-oracle.py` が本物の Vim から生成）の全項目を再生し、QLT-009 の分岐カバレッジの対象。oracle スクリプトの実行は
  QLT-013 の環境依存の確認として `docs/quality/gate-proofs.md` 5-g に記録する（CI に Vim は無い。生成物と `fixtures.json` の一致の検査は CNF-010 で active・Issue #44）
- **不能**: 「Vim のコードを写していない」こと自体。レビューと、Vim の版を上げても fixture 以外に変更が要らないことで担保する

## 結果

得られるもの:

- 単体 exe のまま Vim 操作が載る。テキストの正本は 1 本で、通常モードとの切り替えはキー割り当ての入れ替えだけ（ARC-001）
- 再現度が「人の記憶」ではなく oracle との一致で決まる。難所（`.` の再生・undo の区切り・全角とタブの矩形）は fixture を足せば測れる

失うもの:

- Vim 正規表現・`:g`・`:s` の方言を自分で書く。T2 の範囲で止め、T3 はやらない
- oracle の実行は Windows の Vim に依存する。CI は fixture の再生だけを行い、oracle を回さない

正直に記録しておくこと:

- V2 で測ったのは Ex silent mode で `normal!` が決定的に動くことだけ。挿入モードの `Ctrl-w` や IME 変換中の鍵の扱いは oracle に
  流せない（GUI の事象）ので、そこは fixture ではなく実機確認になる
- Vim 9.1 の既定動作を oracle にするので、`nocompatible` 相当で起動する。`-u NONE` は `compatible` になるため、`probe.vim` の先頭で
  `set nocompatible` を明示している。この 1 行が oracle の「設定」のすべて

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| Neovim / libvim を埋め込む | 施主決定 D3。単体 exe とテキスト正本 1 本が崩れ、Vim のライセンス（Vim license・GPL 互換）が MIT の exe に混ざる |
| VSCodeVim を移植する | TypeScript の実装を C++ に写すことになり、MIT でも「写経」になる。T2 の物差しとして対応表を比較材料に使うだけにする |
| 手書きの期待値だけで単体テストする | 期待値が「自分の理解」になり、oracle との差分が測れない。fixture の期待値は必ず Vim から生成する |
| CI で毎回 Vim を回す | CI に Vim の版を固定して入れる経路が増える。fixture の再生で足り、再生成だけ手元で行う |

## 関連

- SPECIFICATION.md 第 4 節（Vim の範囲 T1 / T2 / T3）
- `eng/measure-language.ps1` の V2 節（oracle の起動引数と probe.vim）
