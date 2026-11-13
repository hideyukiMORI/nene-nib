# 開発ワークフロー — NeNe Nib

> Status: normative（規範）/ 2026-09-15 初版

ワークフローは制約系の一部である。**追跡可能な手順の外で作られた技術的に正しいパッチは、未完成である。**

---

## 1. 作業の単位

production コード・ビルド・依存・方針・利用者向けドキュメントの変更は、すべて 1 つの焦点を持つ Issue から始める。Issue には次を書く。

- 問題と根拠
- 意図する結果
- 影響する規則 ID とモジュール
- 受け入れ条件
- 検証の計画
- **やらないこと**

読み取りだけで、リポジトリを変更しない調査は Issue 無しで行ってよい。

---

## 2. 標準の流れ

1. 焦点を持つ Issue を 1 つ作る／選ぶ
2. 正本のドキュメントと関係する ADR・waiver を読む
3. **編集の前に**正典の実装経路を特定する
4. 最新の `main` からブランチを切る
5. 設計そのものを変えるなら、**先に**方針文書か ADR を変える
6. 最小で完結する変更を実装する
7. テストを足す／更新する
8. 差分・直接の依存と呼び出し元に必要な最小の検証を選び、退行の根拠と結果を記録する。成功済みで関連入力が不変なら再利用する
9. PR を **draft** で出す
10. 影響した規則 ID を 1 つずつ自己レビューする
11. 必要な検証と再利用根拠を PR に記録して Ready にする。工程・文書・SHA の変更だけで測り直さない
12. 記録の妥当性をレビューし、下記の`eng/merge-pr.py`でsquash mergeする。CIはこの実行時だけ起動し、文書のみなら起動しない（QLT-012・ADR 0118）。レビュー済みの PR が並ぶときは、まとめ枝に 1 PR = 1 commit で積んで手元の検証と CI を 1 回にし、`--integration` で rebase merge する（[ADR 0021 追補](adr/0021-diff-scoped-verification-and-result-reuse.md)・#1220）
13. 作業木を畳み、ローカル枝を消す（remote 枝はマージで自動的に消える）

---

## 3. ブランチとコミット

### 統合件名をPRタイトルから取得する（#384）

PR383の統合時に、正しいPRtitleのIssue番号382を手指定のsquash subjectでPR番号383へ取り違えた。
main38bc5b6の履歴は改変せず、PR/Issue/日報へ関係を残す。
`eng/merge-pr.py`を統合入口とし、GitHubから取得したPRtitleを既存`git-conventions.py`で検証して、一字不変でsubjectへ渡す。
PR番号と期待headを必須にし、返却番号/OPEN/ready/main/head一致、titleとClosesのIssue一致を確認する。
完全な変更一覧とrename前のパスから文書のみかを判定する。非文書変更はマージ直前に手動workflowを起動し、対象runのPR/head/repository/workflow一致と成功を確認する（#430・ADR 0118）。
対象repoは道具の一つの定数`github.com/hideyukiMORI/nene-nib`から全read/mergeへ明示し、`GH_REPO`やdefault remoteに委ねない。
branch rulesを同repoから読み、未取得/不正やmerge queueがある場合は止める。件名の単行判定は既存validatorと同じ`splitlines()`で行う。
既定はplan出力、明示`--execute`だけで`gh pr merge --squash --match-head-commit`へ進む。任意subject/管理者override/auto/branch削除は設けない。
引数はshellを通さず配列で渡し、失敗を再試行しない（例外: merge の呼び出しが GitHub の 5xx / `Merge already in progress` で落ち PR が OPEN かつ head 不変なら、30 秒ごとに最大 10 回同じ命令を呼び直す・新しい CI は起こさない・ADR 0021 追補 決定 8・#1223）。実行中のtimeout等は成否不明として、PRの実状態を読んでから次を判断する。
CI成功は検証記録の真実性を保証しないため、手順11〜12のレビューを省略しない。文書のみなら差分と必要な目視で十分で、代替検証スクリプトや長い記録を作らない。ただし文書のみの merge は CI を起動しないので、PR 本文の `検証（実行したコマンドと結果）:` の行に `python eng/conformance.py → 0 violation(s)` を記録する。無ければ `merge-pr.py --execute` が拒否する（#648・ADR 0118）。

```text
python eng/merge-pr.py <PR番号> --expected-head <40桁のhead SHA>
python eng/merge-pr.py <PR番号> --expected-head <同じhead SHA> --execute
python eng/merge-pr.py <まとめ枝のPR番号> --expected-head <head SHA> --integration [--execute]
```

最初のplanに表示された対象/head/件名/CI要否を確認する。planはCIを起動しない。実行前後でPRとrulesを読み直し、headの競合はGitHub側の`--match-head-commit`で拒否する。
CI待機が切れた場合は表示された`--ci-run <ID>`を同じ指定に加えて結果を確認し、CIを重複起動しない。
まとめ枝の道具が merge の後で止まったら（終了 2・`unfinished` の行）、手で PR を閉じる前に `unfinished` の `merged` `mergeCommit` `mergeBase` と `resume` の命令を読む（ADR 0021 追補 決定 9・#1345）。
`resume` の命令（`--resume-after-merge --merge-base <sha>`）は merge も CI もせず、main の列が積んだ commit と全部一致するときだけ開いたままの元の PR を閉じる。まず `--execute` を外して照合だけ流す。
続きの口も拒んだら（`Resume refused`）道具では閉じず、main を確かめてから手で閉じ、その結果を記録に残す。
終了0だけで完了とはせず、同repoのPR番号/headがMERGEDでmergeCommitが得られたことを読み取る。未確認なら再試行せず成否不明として扱う。
PR metadata/rulesの読取と統合は原子的ではないため、操作中にこれらを並行変更しない。競合時に自動復旧やqueueの取消は行わない。
GitHub UIや直接のgh操作まで機械強制するものではなく、規約のplanned/不能の区別は維持する。

Issue・ブランチ・コミット・PR の形は [COMMIT_CONVENTIONS.md](COMMIT_CONVENTIONS.md)（GIT-001〜004）だけが定める。ここには複製しない。
1 つの PR にアーキテクチャ移行・無関係な整理・依存更新・機能追加を混ぜない。

---

## 4. 決定の記録（ADR）

### ADR が要るもの

- 憲章規則・依存方向の変更
- モジュール境界・状態の所有者・隔離区画の変更
- 公開 API の変更
- 保存形式の方針
- 合成／DI の方針
- 並行性・スレッドモデル
- 整形器・静的解析器・テスト枠組み・主要なビルドプラグインの変更
- 対応プラットフォームの変更
- 品質ゲートの重大度・閾値・除外の変更（QLT-010）
- 実行時依存の追加

### ADR が要らないもの

- 承認済みの境界の内側に閉じた実装の詳細
- 文書化された振る舞いを回復するだけの不具合修正
- 方針を変えないテストとドキュメント
- 契約に影響しない小さな依存更新
- **ゲートを強める変更**（弱める変更だけが ADR 事項）

---

## 5. 規則を変える手順

MUST / MUST NOT を変えるには次がそろう必要がある。

1. 矛盾と根拠を書いた Issue
2. 受理された ADR
3. 正本ドキュメントの更新
4. 強制（ゲート）の更新
5. コードの移行
6. **旧経路が拒否されることを示す回帰テスト**

これらは同時に入るのが原則である。段階移行が避けられないときは、期限付きの waiver を作って二重状態を明示する。

`V2` / `Legacy` / `New` / `Alternative` という名の並行実装を作らない。2 つの経路を保つ feature flag には期限と撤去の証明を持つ ADR が要る。

---

## 6. 依存の方針

新しい実行時依存を受け入れるのは、次がすべて満たされるときだけである。

- その機能が具体的にいま必要である
- 自前で書くほうが危険か、保守しにくい
- ライセンスと保守状況が受け入れられる
- 正しい境界の裏に隔離できる
- 版が固定でき再現可能である（QLT-011）
- ADR がある

**便利だから、は理由にならない。**

---

## 7. PR に書くこと

```text
Issue:
目的:
使った正典経路:
規則 ID:
振る舞い・スキーマの変更:
検証（実行したコマンドと結果）:
確認する退行: 今回の差分で壊れ得る挙動
対象・依存: 検証した実装・テスト・関連依存と必要な環境
検証結果: コマンド・結果・ログや実測記録の所在
再利用: 元の記録と関連入力が不変である根拠。初回なら初回検証
Waivers: none | WVR-NNNN
残るリスク:
```

検証の正本は QLT-001 / QLT-012。各テストの必要性を説明できなければ実行しない。
文書・コメント・ルール変更にはアプリの動作テストは不要。開発ツールはその道具の短い正例・反例で確認する。
全件を選ぶ場合だけ、限定不能の理由を先に知らせ、`eng/check.ps1 -Full -Reason <理由>` を使う。定型の承認確認は増やさない。
本件に起因する失敗は直す。無関係な既存失敗は根拠と別 Issue を記録して本件を続け、無関係な全件再試行・修正や合格するまでの反復をしない。

Ex の補完の候補の表（`src/core/ExResult.cpp` の `fixed_candidates`・`palette_names`・`document_names` と `src/core/ExMapCommand.cpp` の `candidates`）に行を足す・消す・並べ替える PR は、同じ PR で probe の期待の写し `tests/performance/ExCompletionProbe.cpp` の `fixed_candidates` を製品の並び（固定 → 一覧の命令 → `map` → 文書の命令と `quit!`）のとおりに作り直す。この写しを読む 9 本（`ex-completion-*` 5 本・`completion-prefix-empty-*` 2 本・`palette-choice-*-all-*` 2 本）は `eng/probe-smoke.ps1`（`-Full` でだけ回る）でしか落ちないので、写しと製品の表の一致は `tests/conformance/test_probe_candidates.py`（`eng/test-conformance.py`・CI の check job）が字面で比べて守る（#1182・ADR 0082 の追補）。
fixture・機械の基準値・許可シンボル・規約検査の規則・保存 schema・既存 scope の checks 数に「差分 0」と書くときは、文ではなく `python eng/protected-diff.py --base <base> --head <head> [--build]` の実出力（stdout の要約と `out/protected/<短い SHA>.json` の要点）を検証結果に貼る（Issue #130・ADR 0038 の決定 5）。

レビューは次を拒否する。

- 既存の意味に対する第 2 の経路
- 所有者の書かれていない状態
- 弱められた／飛ばされたゲート
- 範囲の広い抑制
- アダプタ固有の業務ロジック
- 「将来使うかもしれない」公開 API
- 機能変更に紛れ込んだ無関係な整理

---

## 8. ドキュメントの方針

重要な決定を、チャット・Issue のコメント・コミットメッセージ・AI との会話ログだけに残さない。

- 長生きする不変条件 → 正本ドキュメント
- 判断とトレードオフ → ADR
- 一時的な逸脱 → waiver
- いまのタスク状態 → Issue / PR（`docs/todo/current.md` は要約）
- 背景席（Agent）への依頼書の型 → `docs/templates/`（実装席は [implementation-seat-brief.md](templates/implementation-seat-brief.md)・ADR 0038 / 0039）
- コードのコメント → その場の非自明な理由（方針そのものは書かない）

設定ファイルとドキュメントが食い違ったら、**ドキュメントが決定の記録**である。ただし両方がそろうまで merge は止める。

**日報と要約の粒度（Issue #124・2026-09-23 以降の約束。過去の節は書き換えない）**: 日報（`docs/reports/`）の Issue ごとの節は 5〜8 行の要約に留め、コマンド・件数・所要時間・SHA は `docs/quality/gate-proofs.md` の節に書いて日報からは節番号で指す。`docs/todo/current.md` は「運用・いまの Issue と次の順・いまの数字・動かないもの・段階」だけを持ち、Issue の経緯は書かない（1 画面に収める）。`README.md` の「What works today」は FR の粒度で、数字は `current.md` の値に揃える。

**事故が起きたら、直すだけでなく再発を機械で塞ぐ規則を足し、Issue 番号と実測を規則本文に残す。**

---

## 9. リリース

### 実機確認用の Release（配布物ではない）

hide の実機で見え方と速さを見るための Release は **`eng/build-release.ps1` 1 本**で作る（Issue #129・[ADR 0038](adr/0038-model-per-seat-and-scripted-preparation.md) の決定 5。2026-09-22 に同じ手順を 2 回モデルに踏ませたので 3 回目からはスクリプトが正本）。
`pwsh -NoProfile -File eng/build-release.ps1 -Ref main` が `eng/toolchain.ps1` で `CXX=clang-cl` を固定し、`build/release-<短い SHA>` に Ninja の Release を configure して `NeNeNib` だけを build し、
SHA-256・ref・commit・HEAD・所要時間・道具の版を `out/release/<短い SHA>.json` に記録する。**起動はしない**（起動は設計席が `Start-Process` で行う）。
ref を名指ししない呼び方・無い ref・dirty な作業ツリーは何もせず終了 1（QLT-013: 「この exe はどの ref のものか」を偽らない）。ref が HEAD と違うときは `build/` の下に worktree を一時的に作って build し、終わったら消すので本体の作業ツリーは動かない。
ゲートの `build/` と `eng/measure-speed.py` の `build-release/` には触れない。この経路はゲートに載せない（QLT-013: ゲートに Release も display も要らない）。
PE の `TimeDateStamp` だけは link した時刻なので、同じ commit を作り直すと 2 バイトだけ変わる（Issue #129 の実測）。

### 窓の画（PNG）と前後の比較

実機の道具が操作する窓は、`eng/window_driver.py`で起動したPID・窓class・可視性を照合して選ぶ（#448、ARC-001/012・QLT-013）。同classの先頭1窓だけで不在と判断せず、既存の通常版や別の測定プロセスを飛ばして対象を探す。編集窓と確認ダイアログは同じ所有選択を使い、`tests/conformance/test_window_owner.py`が他PID/不可視/不在とstartの接続を検査する。

computer-use 無しで見た目を確かめるときは `python eng/verify-window.py --capture <dir>`（各節の後の `<dir>/<節名>.png`）か `--capture <dir> --keys "ihello<Esc>"`（`before.png` / `after.png` / 本文の矩形の `frames.json`）で撮り、`python eng/compare-frames.py before.png after.png --inside x,y,w,h` が差分の画素数と外接矩形を JSON で返す（Issue #131・閾値は持たない完全一致・PNG は `eng/window_driver.py` が `zlib` だけで読み書きする）。撮るのは画面の合成なので画面中央を空けて 1 回ずつ走らせ、ゲートには載せない（QLT-013）。

### Vim の差分 fuzz

`python eng/vim-fuzz.py --count 300 --seed 1 --out D:/NeNeNib/outputs/<日付>/vim-fuzz-seed1 --shrink` が固定 seed のランダムな本文と鍵列を本物の Vim 9.1（`eng/vim-oracle.py` の `measure` を import）で測り、再生器 `nib_vim_replay`（`tests/unit/VimReplayMain.cpp`・Debug の `build/` で `cmake --build build --target nib_vim_replay`・CTest に載せない）で Nib に流して本文・`line`・`column`・無名レジスタの本文と種類を比べ、`mismatches.json` と `summary.md` を書く（Issue #954）。`--from-fixtures 50` は再生器の自己検査（素の fixture・余白 `so=0` の viewport の fixture・`so>0` の viewport の fixture を各 N 件まで。viewport は再生器が運ぶ 4 鍵だけの件・#1233）。再生器は数の欄が `uint32` を超える行を `skip` にする（#1233）。生成器（`generator=3`・#1038 で `<C-a>` `<C-x>` と `set nrformats=bin,hex`・#1040 で INSERT の `Ctrl-w` `Ctrl-u` `Ctrl-r` の 3 語・#1042 で矩形の `I` `A` `c` `C` `>` `<` の 6 語・#1037 で `<C-o>` `<Tab>` を足した。**生成器 4**（#1135・2026-10-30）は INSERT の中の `<Left>` `<Right>` `<Home>` `<End>` `<BS>`・行頭の `<BS>`・`gI` の語と、結合文字・異体字セレクタ・ZWJ・国旗・行末の全角の本文の材料を足し、`--generator 3|4`（既定 4）で前の表と乱数の消費を保つ。**生成器 5**（#1224・2026-11-04・`--generator 5`）は 30〜60 行の番号つきの本文と seed から決めた窓（`viewport`・`set scrolloff=0|2|3`）で、画面移動の語 `WORDS_5`（`H M L zt zz zb z<CR> z. z- z+ z^ <C-d> <C-u> <C-f> <C-b> <PageUp> <PageDown>` と `d y v` の `H M L`）を混ぜ、oracle の viewport の probe で測って再生器が `first_visible` と `scroll` も比べる（`<C-e>` `<C-y>` は Nib と鍵の記法の表に無いので外す・shrink は語だけ）。**生成器 6**（#1353・2026-11-10・`--generator 6`）は生成器 5 の語と窓に、`WORDS_6`（回数つきの `<C-e>` `<C-y>`（#1232）・`{` `}` `(` `)`（#1316）と `d y c v V` との組・回数は本文の終わりを越える 40 まで・`<Del>`（#1325・NORMAL・VISUAL・INSERT の中・回数の桁を消す `12<Del>x`）・レジスタの前後の回数 `2"a3dd` `"a2p` `2"ayy` `2"a3.`（#1319））を足し、本文の半分を散文（空行で区切った段落・空白だけの行・`.PP` の行・`.` `!` `?` と閉じの `) ] " '`・全角の `。` `！` `？` `」` `）`）にする。`dS` `yS` `vS` と段落の語は生成でも跳びと数える。`[` `]` は作らない（#1342・Nib は全部断るので同じ違いが並ぶだけ）。回数は 200 を越えない（#1341）。開いた Issue がある違い（#1310 `so>0` の `<C-e>` `<C-y>` と後の 1 語だけに縮んだ `first_visible` の違い（`--shrink` の件だけ）・#1344 検索つきのオペレータ）は表 `DEFAULT_KNOWN` が既知に数え、`summary.md` に規則ごとの最小の鍵列を出す（`--no-known-defaults` で外す・閉じた Issue と直した Issue の行は置かない・#1365）。**生成器 3 の `WORDS` に語を足さない**（表の長さと重みの合計が変わると固定 seed の列が変わり過去の走りが再現できない・`WORDS_4` と `WORDS_5` も同じ・`WORDS_6` も同じ・足すのは次の版（生成器 7）・#1233・#1353））は前の語に依る語（`''` `` ` ` `` と `<C-o>` `<Tab>` は跳びの後（`d` `y` `v` の `H M L` を跳びと数えるのは shrink の依存の検査だけで、生成は生成器 5 の列を保つため前の集合で引く・#1233）・`n` `N` は検索の後・`"1`〜`"9` の読みは名前なしの `dd` `cc` の後・`'a` `` `a `` は `ma` の後）を状態で絞り、`--exclude <語>...` は語を名前で外し、`--known <Issue>:<語>,<語>`（複数可）はその語を含む不一致を既知として別に数え、`--seeds 1-5` は seed ごとに `<out>/seed-N/` と合計の `<out>/summary.md` を書く（新規の不一致は同じ語の列（最小化した件はその語）で畳んで種類ごとの件数と名前を出し、詳しくは各種類の最初の 1 件だけ・#1233・拒否は `rejected.json` に Vim のエラーと拒んだ語・Issue #963）。`--change` は oracle に Vim の変更の印（`&modified`・`b:changedtick`・`undotree().seq_cur` が進んだか）も測らせ、再生器が未保存の印・`'.`・undo の単位を比べる（#1440・ADR 0181・既定は測らない）。Vim が要るので CI とゲートには載せず、見つけた違いは Issue に起こして直す。

### 生成物

手で書かない生成物と、書き直す道具は次のとおり（一致は CNF-010）。`tests/vim/VimFixtures.hpp` は `python -X utf8 eng/vim-oracle.py --regenerate`（`--only <接頭辞>`）、`src/core/VimKeyNames.hpp`（core の表・fixture の再生と設定の `map` が読む・ADR 0176） は `python eng/vim-oracle.py --key-names`、Vim の大小の写像の表 `src/core/CaseMappingTable.hpp` は `python eng/case-mapping.py --write`（固定の Python の `unicodedata` から・`--check` は違えば 1 で終わる・Issue #781）。

製品の版入力は **`CMakeLists.txt` の `project(... VERSION)`** だけとする。設定画面・manifest・VERSIONINFO・配布物名はビルドがそこから導出し、
2 か所目に手で書かない。版は素の `x.y.z`（1.0.0 から・Store の版と同じ）で、**`-beta.N` のような札を版にも配布物の名前にも付けない**（#1596・施主決定 D88・NeNe Folio と同じ形）。
GitHub の配布が試験版であることは、Release の題の「Beta」と pre-release の印だけで示す。`v0.1.0-beta.1`〜`v0.1.0-beta.12` の tag と記録はそのまま残す。
同じ版をもう一度配り直す必要が出たとき（Store の再提出・梱包の作り直しなど）は、**版を上げる**（`1.0.1` か `1.1.0`）。公開した版の配布物を同じ版で作り直さない。

配布物の正規生成経路は次の 1 本である。

```powershell
pwsh -NoProfile -File eng/package-release.ps1 -OutputDirectory D:/NeNeNib/releases/v<x.y.z> -Verification docs/quality/<accepted-record>.md
```

Issue #413 / ADR 0115 / #1596。数値版はCMakeから導出し、配布物の名前は `NeNeNib-<x.y.z>-windows-x64.zip` の 1 通り（`archive_name`・版に `-` が入っていれば道具が断る）。cleanなmain（同じcommitのD側detached作業木も可）とGitHub上の現在のmainの一致を要求する。
`-Preview`はcleanな作業ブランチで梱包ツールを確かめるためだけに使い、公開用として扱わない。
このコマンドは固定した toolchain と既存`eng/build-release.ps1 -Ref HEAD`で **単一ゲートとは別のビルドディレクトリ** に clean build し、追跡済みの規約検査とテストの記録を参照し、
成果物を検査（アーキテクチャ・宣言外の動的依存が無いこと・埋め込み版の一致・同梱一覧の完全一致）してから
`D:/NeNeNib/releases`または`D:/NeNeNib/outputs`配下の新規ディレクトリへ配布物と `SHA256SUMS` を作る。既存出力・既存Releaseビルド・親のリンク/ジャンクションは拒否し、自動で消さない。配布物の直下は **`NeNeNib.exe`・利用者向け `README.txt`・`LICENSE`・`THIRD_PARTY_NOTICES.txt` だけ**とする。
作成したZIPを新規の`extracted`フォルダへ展開して全bytesを照合し、版・commit・SHA・依存・署名状態・検証記録のSHAを`package.json`へ残す。梱包コマンド自体は起動・タグ作成・公開を行わない。
公開物の検査と開発中の検証は対象を明記する。関連入力が不変の検証は再利用し、最終 HEAD という理由だけで全件を追加しない（QLT-012）。

公開の手順（GitHub と Store は同じ commit の同じ exe・記録の雛形は `docs/quality/release-1.0.0-draft.md`）:

| 順 | 誰 | GitHub（ZIP・試験版） | Microsoft Store（MSIX） |
| --- | --- | --- | --- |
| 1 | 設計席 | 版を上げる PR（`project()` の 1 行と、版を見る文書）を main へ | 同じ |
| 2 | 設計席 | clean な main の detached 作業木で上の梱包のコマンド（Release のビルドは 1 回） | 同じ `build/release-<short>/NeNeNib.exe` から `eng/package-msix.ps1 -OutputDirectory D:/NeNeNib/releases/v<x.y.z>/msix -Zip <ZIP> -Store` |
| 3 | 設計席 | 照合: `package.json`（exe の SHA-256・VERSIONINFO が `x.y.z`）と展開した exe の短い起動 | 照合: `msix.json`（exe の SHA-256 が ZIP と同じ・Identity の Version が `x.y.z.0`・言語は `en-US` が先頭・中身 30 個） |
| 4 | 設計席 | annotated tag `v<x.y.z>` → GitHub Release（**題に「Beta」**・例 `NeNe Nib 1.0.0 Beta`・**pre-release の印**）に ZIP と `SHA256SUMS` を添付し、再取得して照合 | — |
| 5 | hide | — | 自己署名の写しで入れて確かめる・WACK（管理者）→ Partner Center に署名なしの `-store.msix` を上げて申請（[docs/store/msix.md](store/msix.md)・[docs/store/submission.md](store/submission.md)） |
| 6 | 設計席 | 記録を `docs/quality/` に | 認定の後に結果を同じ記録へ |

署名・インストーラ・自動更新は別の焦点 Issue とする。
MSIX の版は `project()` の `x.y.z.0` で、`-Store` は先頭が 0 の版を断る（Issue #1581・ADR 0186）。道具は署名・導入・WACK・申請をしない。手順と hide の手でやることは [docs/store/msix.md](store/msix.md)。
