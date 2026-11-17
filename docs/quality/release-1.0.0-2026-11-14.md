# 1.0.0 の公開（2026-11-14）

公開の記録（雛形 [release-1.0.0-draft.md](release-1.0.0-draft.md) から写した・#1657）。この文書 PR で埋めたのは前半（公開元・根拠 (a)〜(e)・候補の扱い・既知の限界）だけで、「梱包」「GitHub の公開」「Microsoft Store の提出」「公開の結果」は本番の梱包の後の文書 PR で埋める。**この記録を置いた時点では、1.0.0 はまだ公開も申請もしていない**（公開版は `v0.1.0-beta.13`）。**追記（2026-11-17）: 1.0.0 は Store で公開され、GitHub の `v1.0.0` も公開した。結果は下の「公開の結果」。**
1.0.0 は Microsoft Store に出す最初の版（施主決定 D88）。GitHub は同じ exe の ZIP を tag `v1.0.0` の pre-release（題 `NeNe Nib 1.0.0 Beta`）で出す。手順の正本は [DEVELOPMENT_WORKFLOW.md](../DEVELOPMENT_WORKFLOW.md) の公開の節、Store の欄は [submission.md](../store/submission.md) と [msix.md](../store/msix.md)。

**範囲**: 製品のソースは beta.13（`3d3859e6`）に、表示の直し 1 本（#1658）を足した物。beta.13 の後の commit は 6 本（#1648 `411b8cb3` 文書・#1620 `9944470c` 版 1.0.0 と梱包の道具・#1650 `1abf1869` 文書・#1655 `238d56b7` 文書・#1660 `3ba9a991` 文書・#1658 `a4c0d40b` 製品）。`git diff 3d3859e6 238d56b7 -- src third_party` は空で、#1620 で製品のビルドの入力が変わったのは `CMakeLists.txt` の版の 1 行（`project(NeNeNib VERSION 0.1.0 …)` → `1.0.0`）だけ。

**この記録を最初に置いた後（起点 `238d56b7`）に main に入った物**（2026-11-14）:

- **製品の変更 1 本**: #1658（PR #1665・main `a4c0d40badc829cdaeb406f308622aa4231f7a92`・2026-11-14T07:38:28Z に squash merge）。F1 の一覧の鍵の札が `Ctrl+Shift+Spac` で切れる（3 言語）のと、最小の窓のステータスバーでモードの文と行と桁が重なる、の表示の直し。見つけたのは今日の撮影（3 言語の目視・下の「今日の調査」）。鍵の欄は札の実測の幅まで広げ、入らなければ欄ごと出さない。ステータスバーはモードの文の右から 6 DIP（96 DPI で 6 px・120 DPI で 8 px）空かない項目を、行と桁 → 表示形式の印 → 案内の順に落とす。収まるときの配置は変わらず、当たり（`status_bar_hit`）も変えない（ADR 0078 と ADR 0154 の追補 2026-11-14）。出荷を止める種類（落ちる・失う）の不具合ではないが、看板の鍵の名前が切れているのは最初の版の見た目として直す、と設計席が決めて公開の前に入れた。根拠は下の (a)〜(d)。
- **文書の変更 2 本**: #1655（PR #1656・main `238d56b7`・掲載文・PRIVACY・README・msix の手順の食い違いの直し）と #1660（PR #1661・main `3ba9a991`・Store の撮影用の見本 `docs/store/samples/README.md` から表を外した。Markdown プレビューは表を描かない（md4c の表の flag を立てていない仕様）のに見本に表があり、撮影の 2 枚目に生の `|` が出ていたため）。

## 作り直し 2（2026-11-15・この節が前の節より優先）

1.0.0 は未公開のまま、**公開元をもう一度作り直す**。2 つ目の公開元 `f00f6d0faa71b807fc52fcf75cdfe422862d13d6`（#1681 / PR #1682 の merge）から 2026-11-14 22:09 に作った ZIP（`0381385a120bf36563e3b54fe5ec582d9c2cfe787ac85147dde74e733e946ed4`・1,504,178 bytes）・MSIX（`8097ef2043dd290a2814f3ec0c85265b7623673370216cba53d591b495b206b3`・1,623,055 bytes）・exe（`b2831cb67b5eb350b492e03bf6ed4e77fd9d97bcf0e02c71dc16d3d97509dda6`・3,635,712 bytes）は **上げない**（値は `D:/NeNeNib/releases/v1.0.0-superseded-f00f6d0f/` の `zip/package.json` と `zip/SHA256SUMS` `msix/SHA256SUMS` から写した・置き場は同じ所に移して残す・`SUPERSEDED.txt`）。この物の照合 7 項目は通っていた（走り書き `D:/NeNeNib/outputs/20261114/notes.md` の 22:2x）。施主は自己署名で入れていない・tag も公開も申請もしていない。

作り直す理由は 4 本の直し（どれも本文を失う・壊す種類）:

- #1683 は施主の決め（2026-11-14 23:1x「直してからリリースかな。」）。
- #1687 #1688 #1689 は 2026-11-15 の公開前の洗い出し（下の「公開前の洗い出し」）で見つかった。施主は終日不在で、**設計席が #1672 #1683 と同じ基準（本文を失う・壊す種類）で 1.0.0 に入れると決めた**（走り書き `D:/NeNeNib/outputs/20261115/notes.md` の 09:0x と 09:33）。**この 3 本は施主がまだ見ていない。tag の前なので、施主が違う考えなら外して作り直せる。**

**新しい公開元**: この節を足した文書 PR（#1709）の merge の commit（40 桁は merge の後に `git rev-parse origin/main` から写し、公開の結果の節に書く）。下の「公開元」の表の 1 行目と「梱包の作業木」は、この commit で読み替える。

**足した製品の変更 4 本**（まとめ枝 35・#1693 / PR #1695・rebase merge・2026-11-15T05:49:36Z）。「beta.13 にもあるか」は、直す前のコードが tag `v0.1.0-beta.13` にあるかを `git show v0.1.0-beta.13:<path>` で確かめ、加えて触ったファイルの `git diff v0.1.0-beta.13 f00f6d0f -- <path>` が空であることを確かめた。

- **#1683（PR #1684・main `609660671355bc9f4b3f728b1a35de1d727e8d62`）— オペレータ待ちの `:s` の起点（本文が消える・不正な UTF-8 が入る）**: Vim モードで `d` `y` `c` などの後に `:s` を打つと、起点が置換の前の Offset のままで、置換で本文の長さが変わるとずれる。`aaaa\nb` に `$d:s/a\+/X/<CR>` は Vim 9.1 で `X\nb`、直す前の Nib は本文が全部消える。`aaaa\nbbbb\ncccc` に `jjlld:1s/a\+/X/<CR>` は Vim `cc`、Nib は全部消える。全角では `あ\nうう あ` に `jly:%s/あ/X/g<CR>P` が Vim `X\nううう X`、直す前の Nib は 2 行目に不正な UTF-8（`\xe3\x81`）を入れ、本文は `uu` で戻るがレジスタは割れたまま残る。直し: 起点を「行と行頭からのバイト数」（Vim の `oap->start`）で持ち、置換の後に行の外へ出たらオペレータを捨てて続きの鍵を打ち切る（`EditorController::operator_substituted` の 1 か所）。起点が字の途中に落ちたら `d` `c` `y` は捨て（Vim は字を割るが Nib は割らない・設計席の決め）、`>` `<` `gU` `gu` `g~` は次のコードポイントの境目へ切り上げる（Vim 9.1 の実測）。仕分けは core の純関数 `vim_pending_splits_characters`（閉じた `VimOperator` の `switch`）。fixture `substitute-op-origin-` 32 件（うち `mid-` 11 件・main は `mid-` 11 件中 5 件落ちる・V-1684c）・手書きの契約 `verify_operator_substitute_mid_character`（捨てる側 6 形）と `_kept`（4 形）。見つけたのは 2026-11-14 23:0x の独立レビュー V-1673（PR #1673 の `:d` `:y` `:g` `:v`）で、設計席がまとめ枝 34 の木の `nib_vim_replay` で再現させた。全角の形は 2026-11-15 の洗い出し H1 の A-1。ADR 0168 の追補 17。**公開済みの `v0.1.0-beta.13` にもある**（`src/application/EditorControllerCommand.cpp` の `operator_substituted` が `const core::Offset origin = state_.selection().caret;` のまま起点を渡す・1.0.0 で直る）。
- **#1689（PR #1691・main `654c2fe936ddf9847a94a4e1a00c1f9788db30d9`）— 保存の置き換えの失敗の後始末（新しいバイト列と元のファイルを失う）**: `ReplaceFileW` が失敗すると理由を問わず一時ファイルを消していた。Microsoft の文書（席 R1 が 2026-11-15 に読んだ）では、1176（`ERROR_UNABLE_TO_MOVE_REPLACEMENT`）は元のファイルが無く新しいバイト列は一時ファイルの名前で残り、1177 は一時ファイルが残り元は別の名前になる。だから 1176 では新しいバイト列も元のファイルも、1177 では新しいバイト列がディスクから無くなる（Vim と比べる形ではない）。直し: 後始末を失敗の種類で決める純関数 `placement_cleanup(error, mode)`（`src/adapters/win32/PlacementCleanup`・消す / 目的の名前へ `MoveFileExW` で置き直す / 残す）。1176 は置き直し、通れば保存は成功・通らなければ一時ファイルを残す。1177 は残す。1175・ほかの番号・新規保存は今どおり消す。残骸の回収を「保存の直前」から「置き換えが成功した保存の後」へ動かした（失敗が続く間は残した一時ファイルを消さない・自動退避・設定・一覧・履歴・ブックマークも同じ道）。試験 `nib_adapter_tests --files` 240 → 252 checks（直す前は 1 件落ちる）。1176 / 1177 は実機で起こせず、純関数の試験だけで守る。見つけたのは洗い出し H5 の F9（コードの読み・再現なし）。ADR 0135 の追補（#1689）。**公開済みの `v0.1.0-beta.13` にもある**（`src/adapters/win32/Win32FileAdapter.cpp` が `placement_failure(GetLastError(), mode)` の直後に `DeleteFileW` で一時ファイルを消し、`reclaim_temporaries_of(target)` を保存の前に呼ぶ）。
- **#1688（PR #1692・main `ec4cf5dc719aca69b1e036c5c2727272fbff5dc0`）— 自動退避の本文と形（退避が本文を守らない・別の本文で戻る）**: 自動退避の本文を文書の文字コードで書き、復元でバイト列から文字コードと改行の形を検出し直していた。Shift_JIS の文書で、退避したバイト列がたまたま妥当な UTF-8 だと別の本文で戻る（`93 FA 96 7B C3 BD`（`日本ﾃｽ`・Shift_JIS）から `日本` を消して退避すると `C3 BD` が UTF-8 の `ý` で戻る）。ASCII だけに削った Shift_JIS は UTF-8 で戻る。CP932 で表せない字のある Shift_JIS は 1 回も退避されない（最後の砦 #1625 も同じ道）。改行の無い所まで削った LF の文書は CRLF で戻る（H5 の F2 F3 F4 F8・Vim と比べる形ではない）。直し: 退避の本文は常に UTF-8（BOM なし・本文の改行の形のまま・保存と同じ `encoded_parts` の 1 本）。文字コード・保存の改行の形・本文の改行の形を新しい値 `AutosaveForm` で一覧の行に持ち、復元は core の `TextBuffer::from_utf8(view, LineEnding)` で読んで検出しない（`restored_bundle` の 1 か所）。**一覧 `autosave.v1` の中身の版を 2 に上げた**（ファイル名は同じ）。書く版は、全部の行に形があれば版 2、どの行にも無ければ版 1 のまま縮める（beta.13 までの残骸の forget）、混ざれば `unwritable`。版 1 の残骸は検出で読む。直す前に落ちる数: `--autosave` 113 中 11・`--autosave-restore-property` 507 中 4（1 本目）、codec の直し（2 本目）の前は `nib_autosave` 152 中 8・`nib_adapters_persisted_property` 30 中 2。直した後は `--autosave` 119・`--autosave-restore-property` 507・`--last-resort` 27 が pass、自動退避と persisted の CTest 4 本が 4 / 4。ADR 0137 の追補（#1688）。**公開済みの `v0.1.0-beta.13` にもある**（`src/application/EditorControllerAutosave.cpp` が復元で `core::DetectedText::from_bytes` を呼び、退避を `encoded_parts(text, document.encoding, …)` で書く）。
- **#1687（PR #1694・main `145f19b8e1d65492c9d1b3c02280bd3dd4b12eaa`）— 矩形の INSERT の中でキャレットを動かした後の写し（ほかの行に別のバイト・不正な UTF-8）**: Vim モードの矩形の `I` `A` `$A` `c` `C` の INSERT の中でキャレットを動かしてから打つと、Esc がほかの行へ 1 行目の別のバイトを写していた。`abc\nabc` に `<C-v>jA<Left>Z<Esc>` は Vim 9.1 で `Zabc\nZabc`、直す前の Nib は `Zabc\naabc`。全角では字の途中のバイトを写して不正な UTF-8 になる（`u` で戻る）。洗い出し H3 の k4 1,980 件のうち 246 件が Vim と違い、65 件で不正な UTF-8。直し: 何も打たないうちに動いた `I`（右へ）と `A`（左へ）は、仮想桁の差とバイトの差が同じときだけ打ち始めの桁の `I` に覚えを置き直す（Vim の `op_insert`・置き直すのは `src/core/VimStepBlock.cpp` の `block_insert_moved` の 1 か所）。ほかの形は直す前の読みに、字の境目と 1 行目の外の 2 つの守り（`VimStepInsert.cpp` の写し）を足した。1 行目の行末の `<Del>` で矩形の最後の行が無くなるときは写さない（Vim の `u_save` と同じ）。fixture `block-insert-moved-` 58 件（register 欄の無い 56 件で main は 24 件落ちる・V-1694）・契約 3 形。k4 で Nib の不正な UTF-8 は 65 → 0、「直す前は一致・直した後は不一致」0 件、違いの総数 257 → 159。見つけたのは洗い出し H3 の F1（既存の Issue なし）。ADR 0035 の追補（#1687）。**公開済みの `v0.1.0-beta.13` にもある**（`src/core/VimStepInsert.cpp` の直す前の写しの条件 `if (length <= insert.line_bytes)` が tag にある・`VimStepBlock.cpp` `VimBlockInsert.hpp` も `f00f6d0f` と同じ）。

`git diff f00f6d0f 145f19b8 -- src third_party CMakeLists.txt eng tests` は 36 ファイル（+1685 / −129）で、`git log --oneline f00f6d0f..145f19b8` はこの 4 本と、その前の `44431b53`（#1685・日報と引き継ぎと記録・文書のみ。`git diff f00f6d0f 44431b53 -- src third_party CMakeLists.txt eng tests` は空）の 5 本だけ。この 4 本のほかに製品の差は無い。`CMakeLists.txt` の差は #1689 の翻訳単位 `PlacementCleanup.cpp` を足した 1 行。

### 公開前の洗い出し（2026-11-15・製品を変えていない席）

根拠は昨日の振り返り（開いている Issue を読むだけでは Issue に無い穴を見つけない）。どの席も Vim 9.1 か性質の試験で実測し、製品は直していない。**「見つからなかった」は、どれも測った集合の中の話**。重さは A（本文が壊れる・不正な UTF-8）・B（未保存なのに保存済み）・C（落ちる・止まる）・D（Vim と違うが A〜C でない）。

- H1（オペレータ待ち × 入力行と `:s` の族・main 相当の Debug の再生器）: Vim 9.1 で測った **2,902 行**（一致 2,287・違い 615・Vim が測れない 13 行は別）。A は 1 種類 = #1683 の族（上の全角の形・PR #1684 の作業中の木では最小化 24 件が全部 Vim と合う）。**B 0・C 0**。D は約 400 行 12 種類（入力行の `<C-r>{reg}`・`<NL>`・相対の番地 `:+2` `:-1`・`:normal` `:u` `:redo` ほか → #1696）。比べられなかった物: `:normal!` の中の `<Esc>` 68 行・入力行の履歴・CRLF の保存のバイト・16,384 字を超える行・保存のバイト列。所在 `D:/NeNeNib/outputs/20261115/hunt-h1/report.md`・道具 `D:/NeNeNib/temp/hunt-h1/`。
- H2（保存の印と undo の不変条件・性質の試験 `--hunt-saved-undo-property`・main `44431b53`・Debug・ASan と UBSan）: **18,000 列**（seed 20261115〜26・5〜60 手）。保存の印 S1・undo の往復 U1・キャレット U2 の破れは **0**。#1672 の直しの前の木（`f749283d`）では 240 列中 23 列で S1 が落ちる（試験が効くことの確かめ）。ASan の access violation が 1 回だけ出た → H2b（**95,000 組**・191 本とも rc 0・不変条件の破れ 0・ASan / UBSan の報告 0）で**再現しなかった**。元の落ちは途中版の binary で、復元した stack の落ちた命令はフレームの slot を読むだけの `movq`（access は読みでも書きでもない・番地は pc と下 16 bit が同じ）＝ ASan の計装を通った bad pointer の報告ではなく、席は混んだ機械での一過性と読んだ。製品の穴の根拠は無い。試験は枝 `test/hunt-saved-undo-property`（未 push・main に入れるのは #1708）。所在 `D:/NeNeNib/outputs/20261115/hunt-h2/`・`D:/NeNeNib/temp/hunt-h2b/`。
- H3（マクロ・レジスタ・矩形・`.`・Vim 9.1 の実測）: 打鍵の道 **8,725 件** ＋ `:normal!` の道 **4,677 件**。A は 1 種類 = #1687。**B 0**。C の候補 1（`x9999999p` が Debug で 30 秒超・Vim は 0.065 秒・Release は未測定 → #1698）。D 754 件（新しい種類: オペレータの後の `v` `V`・`2u` の回数 → #1697）。比べられなかった物: 保存したバイト列・1 GB を超える確保・Release・窓・`"+` `"*`。所在 `D:/NeNeNib/outputs/20261115/hunt-h3/report.md`・`D:/NeNeNib/temp/hunt-h3/`。
- H4（全角・結合文字・絵文字・Tab の本文で字を割る編集・Vim 9.1 の実測）: 打鍵の道 **12,336 件**（本文 12 種類）。#1683 と #1687 の形を除いた集合で、**Nib の本文にもレジスタにも不正な UTF-8 は 0 件**・`u` 10 回で Vim は戻り Nib は戻らない件 0・B 0（保存の照合は回していない）・C 0。席が A 寄りと書いた回数つき INSERT の `<Del>` の後の `u`（`2a<Del><Esc>u` が `uu` で戻る）は本文を失わないので設計席は D と読んだ（#1699）。D 404 件。所在 `D:/NeNeNib/outputs/20261115/hunt-h4/findings.tsv` `tally.tsv`・`D:/NeNeNib/temp/hunt-h4/`。
- H5（開く・保存・退避の道の往復の不変条件・読みと試験・Vim との比較は無い）: R1 替え玉 **117 形**（黙って変わる 0）・実の CP932 **23,564 形**（断る 2,388・黙って書き換わる 398 組 → #1690）・R2 **13 形**・R3 **22 形**・R4（実ファイル）。見つけた物: #1688（F2 F3 F4 F8）・#1689（F9・読みだけ）・#1690（F1 と F5〜F7）。保存の失敗は本文・未保存の印・知らせを保ち、回収は他人の一時ファイルを消さない（測った形の中）。試験は枝 `test/hunt-file-roundtrip`（`063b08d2`・未 push）。所在 `D:/NeNeNib/outputs/20261115/hunt-h5/findings.tsv` `cp932-silent-pairs.tsv`・`D:/NeNeNib/temp/hunt-h5/`。

**根拠**（前の (a')〜(d') に足す。beta.13・#1658・#1672・#1677 の根拠はそのまま再利用する）:

- (a'') 速さのゲート（QLT-014）: まとめ枝 35 の木 `42371ae5a7207ad2337b6ec5702302718fe6952a` の Release（`D:/NeNeNib/worktrees/1693-integration/build-release/NeNeNib.exe` 3,642,880 bytes・SHA-256 `b8056fa9efce8bca30832bcff2de05be84fa2ebc9b2ed5a8687c6f66200ecc77`・この記録の席が測り直した）で **8 benches checked・0 regression・0 unmeasurable**（2026-11-15 14:04〜14:05・5 runs・中央値 起動 41.357 / 窓 29.490 / 単打 0.608 / 200 打鍵 2.915 / open 16 MiB 51.928 / 16 MiB で 200 打鍵 3.024 / 面 1.031 / 長い行 1.089 ms・ビルドの process 0・負荷の読み 4 / 1 / 2%・記録 `D:/NeNeNib/outputs/20261115/gate-integration35-42371ae5.log` と `-load.txt`・`out/speed/2026-11-15T05-05-52Z.json`・1 回だけ測った）。施主は終日外回りで、机に誰もいない間に測った。`git diff 42371ae5 145f19b8` は空（rebase merge で SHA だけが変わった）。
- (b''') 独立レビュー（読むだけの席・Vim 9.1 と probe で実測）:
  - #1684: V-1684（`D:/NeNeNib/outputs/20261114/review-1684/review.md`・直してから・必須 M1 = 起点をコードポイントで比べ main で合っていた形を壊す）→ 席 O1b（バイトへ）→ V-1684b（`D:/NeNeNib/outputs/20261115/review-1684b/review.md`・Vim 9.1 で新しい形 86 件ほか・直してから・必須 N1 = どのオペレータでも捨てると `>` `<` で main より悪くなる）→ 席 O1d（`d` `c` `y` だけ捨てる）→ V-1684c（`review-1684c/review.md`・**merge してよい**・必須 0・軽い 1・後続 3・「main で合い PR で違う」形 0 件・`VimOperator` 8 値の仕分けを Vim で 1 形ずつ測って合う）→ 軽い L1（注釈）は席 O1e が `a67ca7b1` で直した。
  - #1691: V-1691（`D:/NeNeNib/outputs/20261115/review-1691/review.md`・**merge してよい**・必須 0・軽い 4・後続 3・Microsoft の文書を読み直して表と一致・実機の probe で読み取り専用 5・ロック 32 ほか 4 形で両方のファイルが残る・`--files` 252 / 0）→ 軽い 4 は席 R1b が `72fae166` で直した（文書と試験の文と CLAUDE.md の見出しだけ）。
  - #1692: V-1692（`D:/NeNeNib/outputs/20261115/review-1692/review.md`・直してから・必須 2 = beta.13 までの版 1 の一覧が 2 行以上あると forget のたびに `unwritable`・CTest `nib_adapters_persisted_property` が落ちる）→ 席 A1b（codec が行の形の有無で版を決める）→ V-1692b（`review-1692b/review.md`・**merge してよい**・必須 0・軽い 0・後続 2・本物の `Win32FileAdapter` と beta.13 の codec の写しを同じ exe に入れた probe 43 checks・0 失敗）。
  - #1694: V-1694（`D:/NeNeNib/outputs/20261115/review-1694/review.md`・**merge してよい**・必須 0・軽い 2・後続 4・3,275 件（k4 1,980 ＋ 新しい形 1,295）で PR の不正な UTF-8 0 件（main は 110）・「main は一致、PR は不一致」0 件）。軽い L1（キャレット）は #1706、L2（1 本目の commit の本文）はまとめ枝の commit に影響しない。
- (b'''') 手元の検証（まとめ枝 35・HEAD `42371ae5`）: Debug の全 target（warning 0・build 1057 s）・`ctest` 22 / 22（1218.04 s）・`python eng/symbols.py` 0 violation（3 libraries）・`python eng/conformance.py` 0 violation・`python eng/test-conformance.py` OK・`eng/format-check.ps1` 通過・Release の全 target（warning 0）（合計 3078 s・`D:/NeNeNib/outputs/20261115/verify-1693.log`）。表は contracts 113・scopes 139・groups 40（数え直して宣言と一致）・fixture 6,041 件（名前の重複 0）。
- (c'') CI: PR #1695 の merge の道具の run [43679717232](https://github.com/hideyukiMORI/nene-nib/actions/runs/43679717232)（`workflow_dispatch`・題 `Merge PR #1695 at 42371ae5a7207ad2337b6ec5702302718fe6952a`・success・2026-11-15T05:07:39Z〜05:49:26Z）→ rebase merge で main `145f19b8`（2026-11-15T05:49:36Z）。これが製品の最後の CI。元の PR #1684 #1691 #1692 #1694 は道具が閉じた。この記録の PR は文書のみ。
- (d'') 施主の試用（D41）: **4 本とも施主はまだ見ていない**（終日不在）。2026-11-14 20:3x に渡した試用の exe（(d') の A 版と署名・B 保存の印・C 朝の表示の直し）の結果も、この記録を書いた時点で受け取っていない。施主は本番の物で、packaged の確かめと一緒に見る。直しが要れば、未公開なので同じ版のまま公開元をもう一度作り直す。

**既知の限界に足す**（下の「既知の限界」の節にも同じ物を足した）:

- Shift_JIS の拡張文字（NEC 選定 IBM 拡張 `0xED40-0xEEFC` 374 組・`0x8790-0x879C` 9 組・`0xFA4A-0xFA5B` 15 組の計 398 組）は、無編集で保存しても Windows の正準の符号に書き換わる（字は同じ）。洗い出し H5 が実の CP932 で測って確かめた（#1690・止めない・Windows の標準の見込み）。
- 改行の手前の literal CR（`^M`）と保存の改行の形の切り替えの組み合わせでは、開き直すと literal CR が改行に混ざる・改行の形の検出が戻る、などバイト列が往復で変わる稀な形がある（#1690・H5 の F5〜F7）。
- 自動退避の一覧 `autosave.v1` の中身の版が 2 になった（#1688）。beta.13 までの Nib は版 2 の退避を問わず、消しもしない（1.0.0 から beta に戻すと、1.0.0 が残した退避は戻らないがフォルダに残る）。文字コードや改行の形だけを選び直した文書は退避し直されず、落ちると古い形で戻る（本文は失わない・前から・#1704）。「はい」の後の退避の store は非同期・古い残骸の forget は同期で、その間（ms の幅）に落ちると同じ文書が 2 本並びうる（#1705）。
- オペレータ待ちの `:s`（#1683）の Vim との残りの違い: `gU` `gu` `g~` で起点が範囲の始まりになり字の途中に落ちる形は、本文は Vim と同じでキャレットが違う（`aaééx\nc a` の `$hgU:%s/a/ab/<CR>` は Vim が 1 行目の行頭からバイト 3・Nib はバイト 5）。`d` `c` `y` で起点が字の途中に落ちると、Vim は字を割るが Nib はオペレータを捨てる（設計どおり）。パターンの `\n` が当たらない（#1566）・`d:s` の後の `u` が 2 回要る・レジスタの中の `:s` の行が `@a` で確定しない（どれも main と同じ・#1701 #1696）。
- 矩形の INSERT（#1687）の Vim との残りの違い: `$A` で動いてから打つ形・全角や Tab をまたいで動いた形・`<C-o>` で抜けた後（#1544）・Vim 自身が不正な UTF-8 を作る形は Vim と違う（どれも妥当な UTF-8・main と同じか良くなった）。`<Del>` で矩形の最後の行が無くなる形の Esc の後のキャレット・`3I` のキャレット（#1706）。矩形の `c` `C` の後の `u` を Vim は一部だけ戻し Nib は全部を戻す（#1707）。
- 保存の後始末（#1689）: 1176 / 1177 は実機で起こせず、純関数の試験だけで守る。1176 の置き直しの後は属性と ACL を失いうる。1177 の後に OS が別の名前で残した元のファイルは回収しない。起動時の設定の置き場の回収は前のプロセスが残した一時ファイルを消す（変えていない・#1702）。

## 作り直し（2026-11-14 夜・下の節より優先・上の「作り直し 2」が更に優先）

1.0.0 は未公開のまま、**公開元を作り直す**（施主の決め 2026-11-14 夜: 「再現したら 1.0.0 に入れて作り直して」「作り直すなら版と署名を入れたい」）。前の公開元 `672c15c58cc32f5b076cc9e1c20396a6feb272bf`（#1657 / PR #1659 の merge）から作った ZIP（`ff349e84…dcb2d`）と MSIX（`abb6a407…661a7`・exe `cc72edbf…64a1`）は **上げない**（置き場を `D:/NeNeNib/releases/v1.0.0-superseded-672c15c5/` に移して残す・施主はまだ自己署名で入れていない・tag も公開も申請もしていない）。

**新しい公開元**: この節を足した文書 PR（#1681）の merge の commit（40 桁は merge の後に `git rev-parse origin/main` から写し、公開の結果の節に書く）。下の「公開元」の表の 1 行目と「梱包の作業木」は、この commit で読み替える。

**足した製品の変更 2 本**（まとめ枝 34・#1679 / PR #1680・rebase merge・2026-11-14T13:01:24Z）:

- **#1672（PR #1675・main `7290a93d0796b3f192bad3cbc9d559a0cb04b071`）— 保存の印の直し（本文を失う種類）**: 履歴の単位を畳む 2 つの口で、保存時点の位置が畳む前の値のまま残り、保存の後に変えた本文が「保存済み」と判定されていた（● が出ず、閉じるときに「保存しますか」を聞かれない）。口は (1) マクロや `.` の再生の途中の `:w`（`merge_replayed_edits`）と (2) 矩形の INSERT の途中の Ctrl+S の後の Esc（`fold_block_into_insert`）。保存時点を畳んだ履歴へ写す純関数 `after_units_folded`（`src/application/Document.hpp`）の 1 本にした（scope `--fold-save-point`）。見つけたのは今日の独立レビュー（V-1666 が `:s` の `c` の畳みを読む途中）で、設計席が main の製品で再現させた。**公開済みの `v0.1.0-beta.13` にもある**（1.0.0 で直る）。REL-A の「止める製品の不具合 0 件」は開いている Issue を読んだ結果で、この不具合は Issue に無かった。
- **#1677（PR #1678・main `0747a19a3982b5e2503f46a4ad22bc895bb39766`）— バージョン情報（施主決定 D90）**: F1 の操作「バージョン情報」と Ex の `:ve` `:ver` `:version` がステータスバーに 1 行（版・署名・GitHub の URL の文字）、起動画面は題の右に版・一覧の下に署名の 2 行（入らなければ署名 → 版の順に落とす）、exe の版情報の `CompanyName` は `Hideyuki Mori`。版は `project(... VERSION)` を `src/core/AppIdentity.cpp` にだけ渡す 1 本（CNF-007）。URL は文字だけで、開かない・通信しない・クリップボードに書かない（PRIVACY.md と Store の申告「Web ページを開かない」は変えていない）。Store の 5 枚に起動画面と F1 の一覧は無いので、掲載のスクリーンショットは撮り直さない。

`git diff 672c15c5 0747a19a -- src third_party CMakeLists.txt eng tests` は 47 ファイル（+1342 / −51）で、この 2 本のほかに製品の差は無い（間の #1670 / PR #1671 `f749283d` は文書のみ）。

**根拠**（前の (a)〜(e) に足す。beta.13 と #1658 の根拠はそのまま再利用する）:

- (a') 速さのゲート（QLT-014）: まとめ枝 34 の木 `dfbf1840be4f303fca2f5d100cad9dd75c882bcc` の Release（`NeNeNib.exe` 3,635,712 bytes・SHA-256 `e6ba3aea091a7a960af853c814b1362c219f4de7410a13804c7d23538917c9e9`）で **8 benches checked・0 regression・0 unmeasurable**（2026-11-14 21:28〜21:32・施主の了承の後・5 runs・中央値 起動 46.443 / 窓 35.140 / 単打 0.625 / 200 打鍵 3.009 / open 16 MiB 58.304 / 16 MiB で 200 打鍵 3.226 / 面 1.056 / 長い行 1.128 ms・ビルドの process 0・負荷の読み 0 / 6 / 0%・記録 `D:/NeNeNib/outputs/20261114/gate-integration34-dfbf1840.log` と `-load.txt`・1 回だけ測った）。`git diff dfbf1840 0747a19a` は空（rebase merge で SHA だけが変わった）。起動画面に版と署名の layout が足されたので、起動の 2 本が基準の内にあることを merge の前に確かめた（V-1678 の求め）。
- (b') 独立レビュー（読むだけの席・2026-11-14）: #1675 は V-1675（`D:/NeNeNib/outputs/20261114/review-1675/review.md`・矩形の INSERT の口が残っていると指摘）→ 直しの席が同じ枝に commit を足し（HEAD `6b379b75`）→ V-1675b（`review-1675b/review.md`・「片付いた」）。#1678 は V-1678（`review-1678/review.md`・HEAD `832bd457`・「merge してよい」・必須 0）。V-1678 の軽い指摘 L1: バージョン情報の 1 行は最小の窓（360 DIP）で省略記号なしに切れる。起動画面の署名の 2 行は、履歴の一覧が長いと出ない（D90 の「切らずに落とす」のとおり）。どちらも 1.0.0 を止めず、施主に見てもらう。
- (b'') 手元の検証（まとめ枝 34・HEAD `dfbf1840`）: Debug の全 target（warning 0）・`ctest` 22 / 22・`python eng/symbols.py` 0 violation・`python eng/conformance.py` 0 violation・`python eng/test-conformance.py` OK・`eng/format-check.ps1` 通過・Release の全 target（warning 0）（`D:/NeNeNib/outputs/20261114/verify-1679.log`）。表は contracts 113・scopes 139・groups 40。
- (c') CI: PR #1680 の merge の道具の run [43595457342](https://github.com/hideyukiMORI/nene-nib/actions/runs/43595457342)（`workflow_dispatch`・題 `Merge PR #1680 at dfbf1840be4f303fca2f5d100cad9dd75c882bcc`・success・2026-11-14T12:34:00Z〜13:01:14Z）→ rebase merge で main `0747a19a`。これが製品の最後の CI。この記録の PR は文書のみ。
- (d') 施主の試用（D41）: 試用の exe（`D:/NeNeNib/evidence/20261114-trial/NeNeNib-integration34-dfbf1840.exe`・上の (a') と同じバイト列）と表（同じ所の `README.md`・A 版と署名 7 項目・B 保存の印 4 項目・C 朝の表示の直し 2 項目）を 2026-11-14 20:3x に渡した。**この記録を書いた時点で結果はまだ受け取っていない。** 施主は本番の物でも packaged の確かめと一緒に見る。直しが要れば、未公開なので同じ版のまま公開元をもう一度作り直す。

**既知の限界に足す**: 設定に鍵を足す版（1.0.0 の後）へ上げてから 1.0.0 に戻すと、1.0.0 は知らない鍵のある `settings.v4` を壊れた物として読み、そのセッションの設定の保存を断る（ファイルは壊さない）。1.0.0 の後は、鍵を足す前に形（版を上げる / 読み飛ばして残す）を施主が決める。

## 公開元

| 項目 | 値 |
| --- | --- |
| 公開元の commit（main） | この記録を置いた文書 PR（#1657）の merge の commit（40 桁は merge の後に `git rev-parse origin/main` から写し、公開の結果の節に書く） |
| 前の公開 | `v0.1.0-beta.13`（`3d3859e69fc8023e1374ff4adae5af34ae916c98`・tag object `04b55b0e98fc4e04d3cf5a35a56de862b110a32a`・2026-11-14 00:16・[記録](beta13-release-2026-11-13.md)） |
| 版（`project()`） | `1.0.0`（#1620 / PR #1621・main `9944470c`・2026-11-14 01:46 JST） |
| 梱包の作業木 | 公開元の detached 作業木（HEAD = origin/main = 公開元・未追跡 0）。候補の木 `9944470c` は使わない |

## 候補 `v1.0.0-candidate-9944470c` は予行（上げる物ではない）

`D:/NeNeNib/releases/v1.0.0-candidate-9944470c/` は main `9944470c` から作った ZIP と `-Store` の MSIX で、手順の 1a（この記録の merge の commit から作る）を通っていない（`zip/package.json` の `commit` は `9944470c`・`verification.path` は雛形 `docs/quality/release-1.0.0-draft.md`）。Release の exe は作るたびにバイト列が変わる（PE の時刻が入る）ので、同じソースから作り直しても同じ exe にはならない: 予行（作業枝 `04c2e3ce`・`D:/NeNeNib/evidence/20261114-release-rehearsal/README.md`）の exe は `4dae2ccee1344760b945607e5d17c235848e2bfa7c3c55be6341a96c11b661b1`、候補（`9944470c`）の exe は `12356835469135a5a4937ff3188700f02294671839a43d5b890f937df5a1b1cb`（どちらも 3,623,424 bytes・`git diff 04c2e3ce 9944470c -- src third_party CMakeLists.txt eng tests` は空）。候補を試して申請したあとで GitHub の ZIP を公開元から作り直すと、「GitHub と Store は同じ commit の同じ exe」が崩れ、直すには版を上げるしかない（同じ版で作り直さない）。だから**本番の ZIP と MSIX は公開元で 1 回だけ作り、施主はその本番の物を試して申請する**（試した物 = 上げる物・設計席の決め 2026-11-14・走り書き `D:/NeNeNib/outputs/20261114/notes.md`）。候補は手順と道具の予行としてだけ扱う。施主の試しで直しが要れば、未公開なので同じ版のまま公開元を作り直す。なお候補を作った後に main へ #1658（`a4c0d40b`）が入ったので、候補 `9944470c` の exe は公開する物とバイト列だけでなく製品のコードも違う（`git diff 9944470c a4c0d40b -- src` は 9 ファイル）。

## 根拠（再利用する検証）

製品のソースは beta.13 に #1658 を足した物。#1658 は (a)〜(d) の新しい根拠（ゲート・独立レビュー・CI・席の検証）で受け、それ以外の製品の検証は beta.13 の根拠を再利用する（QLT-012）。版 1.0.0 で変わった物（版の 1 行・梱包の道具・ZIP の `README.txt`・文書）は (b)・(c)・(e) で受ける。

- (a) 速さのゲート（QLT-014）: **#1658 の木 `41ecd0041ffdee00c97087178683bfcefc71d259`** の Release（作業木 `D:/NeNeNib/worktrees/1658-chord-label-and-status-overlap`・HEAD は `41ecd004` で未追跡 0・`NeNeNib.exe` 3,624,960 bytes・SHA-256 `5bbe281d0936fa0a0aaeb09c4944352d857d37a264eb5f7efc02c1a16238b60b`・Release のビルドの `FAILED|error:|warning:` は 0 行）で **8 benches checked・0 regression・0 unmeasurable**（2026-11-14 15:55〜15:56・5 runs・中央値 起動 44.500 / 窓 34.075 / 単打 0.619 / 200 打鍵 2.970 / open 16 MiB 49.808 / 16 MiB で 200 打鍵 2.604 / 面 1.093 / 長い行 0.963 ms・測る前の負荷の読み 0 / 4 / 1%・ビルドの process 0・記録 `D:/NeNeNib/outputs/20261114/gate-fix1658-41ecd004.log` と `-load.txt`・`release-and-gate-1658.log`）。**squash した main の木は、ゲートを測った木と製品の入力で同じ**: `git diff 41ecd004 a4c0d40b -- src third_party tests CMakeLists.txt eng` は空（`41ecd004` と `a4c0d40b` の差は #1660 の文書 2 ファイル `docs/store/samples/README.md` と `docs/store/screenshots.md` だけ）。この後の記録の PR は文書のみ。
  - その前の版の値（製品が beta.13 と同じだった木・記録として残す）: まとめ枝 33 の木 `0e5a2e3e` の Release で **8 benches checked・0 regression・0 unmeasurable**（2026-11-13 23:19〜23:21・中央値 起動 44.293 / 窓 32.818 / 単打 0.541 / 200 打鍵 2.843 / open 16 MiB 52.389 / 16 MiB で 200 打鍵 2.758 / 面 1.084 / 長い行 0.987 ms・負荷 4〜7%・記録 `D:/NeNeNib/outputs/20261113/gate-integration33-0e5a2e3e.log` と `-load.txt`・基準値は [speed-adoption-2026-10-18.md](speed-adoption-2026-10-18.md)）。`git diff 0e5a2e3e cc9c7f44 -- src third_party tests CMakeLists.txt eng` は空。`git diff cc9c7f44 238d56b7 -- src third_party tests CMakeLists.txt eng` は 6 ファイル（`CMakeLists.txt` の版の 1 行・`eng/package-release.ps1` `.py`・`eng/release/README.txt.in`・`eng/test-package-release.py`・`tests/conformance/test_package_msix.py`）で、製品のビルドの入力は版の 1 行だけ（ほかは梱包の道具・梱包の文・試験）。どれも速さに関わらないので、梱包の exe では測らない（beta.9〜beta.13 と同じ形）。
- (b) 独立レビュー:
  - beta.13 までの製品の変更は beta.13 の記録の (b) のとおり（[beta13-release-2026-11-13.md](beta13-release-2026-11-13.md)）。
  - #1658（PR #1665）は **V-1665**（読むだけの席・2026-11-14・`D:/NeNeNib/outputs/20261114/review-1665/review.md`・対象は `70d26dd1c5ba7533009b1d5ea3c9df92ba982a72` を git の object から読んだ・実測なしで数字は式からの読み）。判定は「**70d26dd1 のままなら merge してよい**」（必須 0）で、そのとき別の席が入れていたモードの文と行と桁の間 8 → 6 DIP の変更には**必須 1**（`tests/unit/CoreTests.cpp` の `verify_status_bar_fitted` の `marked` が、6 DIP では limit = 122 + 150 + 6 = 278 で印の左端 278 が `278 < 278` を満たさず落ちる → モードの幅を 151 以上に・ADR 0154 の数字と PR 本文の書き直し）。この必須は次の commit `41ecd004`（間を 6 DIP）で片付いた: `git show 41ecd004` で、`marked` はモードの幅 152（122 + 152 + 6 = 280 が 278 を越えて印を落とす）に直り、境目ちょうどの 150 で印が残る例（`a mark that starts exactly at the gap after the mode text stays`）が足され、ADR 0154 の追補の数字も同じ commit で直っている。R0（退行の反例探し）では、普通の窓で配置は動かない（`fitted == layout`）・当たりは測らない配置のまま・保持の枠は最大 11 のまま・足した費用は保持の枠の `GetMetrics` が 1 frame に 2〜3 回で、5000 件の面（鍵の無い候補）では 0。軽い指摘 5（狭い窓で行と桁が出たり消えたりする・規則が core の 2 関数に重複 ほか）と後続 2（150% / 200% の DPI・利用者の字体・renderer の配線の試験／最小の窓と 734〜770 px の帯の実機の目視）は 1.0.0 の後。
  - #1658 の席の検証（PR #1665 の本文）: 間を 6 DIP にした HEAD `41ecd004` の exe で `nib_window_tests --language` 9,670 checks・0 failures／`nib_window_tests` 既定 15,291 checks・0 failures／`nib_tests` 既定 2,405,548 checks・`--guide` 544・`--command-palette` 886 pass／conformance 0 violation／clang-format の差分 0／Debug の全 target のビルドで警告 0。直す前の答えに一時的に戻すと `--command-palette` 6 件・`--language` の鍵 10 件（前の HEAD で測定）、6 DIP の後にステータスの 2 本だけ戻すと `--language` 1,246 件・`--guide` 2 件が落ちる。測った集合はステータス 1,320 形（3 言語 × モードの文 11 × 行と桁 5 組 × 96 / 120 DPI × 窓 4）と、面は 2 DPI × 窓 3 × 最も広い鍵。`ctest --test-dir build` 22 / 22 pass と symbols 0 violation は前の HEAD `70d26dd1` の値で、`41ecd004` は core の定数 1 つ（`status_text_gap_dips` 8 → 6）と unit の期待 1 か所と ADR の数字だけの差なので再利用した（定数を読む試験は HEAD の exe で流し直した）。
  - 版 1.0.0 と梱包の道具の PR #1621（`9944470c`）は V-1621（読むだけの席・2026-11-14・`D:/NeNeNib/outputs/20261114/review-1621/review.md`）。判定は「公開の前に直す」（必須 2・軽い指摘 5・後続 4）で、道具のコードに公開を止める穴は無い。必須の片付け: **M1**（Store の掲載文 3 言語が「`:s` はまだありません」と書いていた）は #1655 / PR #1656（main `238d56b7`）で直した。**M2**（候補は手順の 1a を通っていない・exe は作るたびにバイト列が変わる）は、本番をこの記録の merge の commit から作り、施主はその本番の物を試す、で片付ける（上の「候補は予行」）。軽い指摘の L2（自己署名の手順の `-store.msix` の名前）と L5（`submission.md` の `MinVersion`）は #1656 で済み、L1（DEVELOPMENT_WORKFLOW の ZIP の出力先）と L3（`entries` の数）はこの PR で直した。L4 と後続 F1〜F4（`-Verification` が雛形を受ける・同じ版の作り直しを機械が止めない・`--store` が ZIP の `package.json` を見ない・`-Store` を通す試験が無い）は 1.0.0 の後の Issue。
  - 出荷前の点検 REL-A（読むだけ・2026-11-14・`D:/NeNeNib/outputs/20261114/rel-a/readiness.md`）: 開いている Issue 97 本を読み、**1.0.0 を止める製品の不具合は 0 件**（落ちる・数秒以上止まる・本文を失う・黙って違う中身で保存する、のどれにも当たる物が無い）。配る物（候補の ZIP 4 個・MSIX）に個人のパスと `.pdb` は 0・import は OS の DLL だけ・`MinVersion` `10.0.22621.0`。外に出す文の食い違い 9 件は #1656 で直した。REL-A が**確かめていないこと**: WACK の結果・自己署名での導入と起動（packaged の振る舞い）・BlockMap の SHA-256 の検査・異常な入力（#519）を Release の exe でまとめて見ること。窓は出しておらず、ビルドも起動もしていない。
- (c) CI: PR #1621 は merge の道具の run [43460142800](https://github.com/hideyukiMORI/nene-nib/actions/runs/43460142800)（`workflow_dispatch`・題 `Merge PR #1621 at 04c2e3ce35ed53e847c4024abccef2654060d023`・`check` と `build-and-test` が success・`check.yml` は `expected_head` を checkout する）→ squash merge で main `9944470c`（2026-11-13T16:45:53Z）。#1658 は PR #1665 の merge の道具の run [43568791652](https://github.com/hideyukiMORI/nene-nib/actions/runs/43568791652)（`workflow_dispatch`・題 `Merge PR #1665 at 41ecd0041ffdee00c97087178683bfcefc71d259`・`check` と `build-and-test` が success・2026-11-14T06:57:20Z〜07:38:23Z）→ squash merge で main `a4c0d40b`（2026-11-14T07:38:28Z）。これが製品の最後の CI（その前は beta.13 の記録の (c)・run 43442097904 → `cc9c7f44`）。#1650 #1656 #1661 とこの記録の PR は文書のみなので、merge 直前の CI は要らない（CLAUDE.md §4）。
- (d) 施主の試用（D41）: 2026-11-13 20:4x〜21:3x の表 A〜D（表示言語・アイコン・Vim の命令・決めてほしいこと）が OK（表 `D:/NeNeNib/evidence/20261113-trial/README.md`・結果は [beta13-release-2026-11-13.md](beta13-release-2026-11-13.md) の (d)・施主決定 D89）。beta.13 までの製品はこの試用を再利用する。**#1658 の直しは施主がまだ見ていない**: 表示の小さな直しで、施主が不在の間に設計席の判断で公開の前に入れた。施主は本番の物で、packaged の確かめと一緒に見る。**packaged（MSIX）の起動と振る舞いの確かめと WACK はまだ**（施主の手・[submission.md](../store/submission.md) の「packaged の実機の確かめ」と [msix.md](../store/msix.md)・本番の `-store.msix` で行う）。
- (e) 梱包の道具の試験: V-1621 が 2026-11-14 に回した `python eng/test-package-release.py --work-dir <D 側の新しいディレクトリ>` → **10 件 OK**（`D:/NeNeNib/outputs/20261114/review-1621/test-package-release.log`）と `python -m unittest tests.conformance.test_package_msix` → **18 件 OK**（`test-package-msix.log`）。`eng/test-package-release.py` は `--work-dir` が要る手動の試験で、CI には載っていない（V-1621 の L4）。

### 今日の調査（2026-11-14・製品を変えていない席）

- 出荷前の点検 REL-A: 上の (b) のとおり（`D:/NeNeNib/outputs/20261114/rel-a/readiness.md`・止める製品の不具合 0 件）。
- 差分 fuzz FZ2（main `1abf1869` の再生器・`D:/NeNeNib/outputs/20261114/fz2/`・席の報告の本文が正で `report.md` は無い）: **9,600 件・落ち 0・止まり 0**。生成器 6（seed 201〜210・3,000 件）一致 99.97%（違い 1 件は閉じた #1483 と同じ形 `zb<C-f>`・画面移動の端で落ちではない・公開の後に確かめる）・生成器 5（3,000 件）100%・Ex の行（FZ1 の生成器・seed 11〜13・比べられる 2,097 件）90.3%・語を足した版（1,079 件）92.0%。違いは既知の種類で、**新しい種類は 1 つ**（`:s|` `:&|` の後の `|` を読まない → #1664・未着手）。
- 撮影 SHOT1（`D:/NeNeNib/evidence/store-1.0.0/`・exe は候補の `12356835…b1cb`・1600 × 1000・120 DPI）: 目視 66 枚（3 言語 × 22・`review/<lang>/`）と Store の下書き 15 枚（3 言語 × 5・`screenshots/<lang>/`）。指摘 12（中 6・軽 6）のうち製品の直しにしたのが #1658、撮影用の見本の表は #1660 で外した。**Store の 2 枚目（Markdown プレビュー）と目視の 08 は本番の exe で撮り直す**（下書きの 2 枚目には表の生の `|` が写っている）。

## 既知の限界

beta.13 の記録の「既知の限界」をそのまま引き継ぐ（製品は beta.13 ＋ #1658）。それに加えて、REL-A が 1.0.0 を止めないと決めた物（`D:/NeNeNib/outputs/20261114/rel-a/readiness.md` の A 節）:

- #1347: 2 つの Nib が閉じる時と起動する時に重なると、片方の `session.v2` を上書きしうる。`history.v1` も lock の無い「読む → 足す → 書く」で、片方の追記を失いうる。失うのはタブの一覧と履歴で、文書の本文ではない。
- #1583: Issue に書いた形（入力行が開いたまま本文が入れ替わる）は、今日の席と独立レビュー V-1662 が開く入口を全部たどって再現しないと確かめた（入力行の間は読みを留める）。代わりに見つかった穴: 起動の復元で見ていたタブを裏で読んでいる間に、仮の無題へ別のファイルを開くと、復元のタブが揃った時点でそちらへ切り替わる（ADR 0159 追補 6 の破れ・失う本文は無い）。直しは PR #1662（レビュー済み・1.0.0 には入れていない）。
- #1430 (1): Markdown プレビューの画像が OneDrive の「オンラインのみ」・割り当てたネットワークドライブ・UNC の先にあると、worker が数十秒止まりうる（UI のスレッドは止まらない・自動退避と開く道の読みとフォルダの列挙が待たされる）。1.0.1 の候補。
- #1631: 最後の砦は C++ の例外だけを受け、SEH（アクセス違反・stack の溢れ）では退避しない（30 秒ごとの自動退避は残る）。
- 作った入力や巨大な回数で重くなる物: 16 MiB で空白の無い 1 行の `iw` `aw`（#1441・0.4〜0.9 s の読み）・行が伸びるマクロの巨大な回数（#1451・再生は 100,000 鍵で止まる D75）・巨大な回数の `N` `gN`（#1632 F3）・病的な検索の形の `hlsearch` と incsearch（#1588 F3・#1619 L6・仕事の予算は 1.0.1 以降）。
- #739（260 文字を超えるパスのファイルダイアログの道）と #424（D ドライブの隔離 profile で設定の保存の後に応答しなくなった 1 回・原因未確定）と #519（異常な入力の Release での調査）は確かめていない。
- Shift_JIS の二重の符号: CP932 で同じ字に 2 つの符号がある所（NEC 選定 IBM 拡張 `0xED40-0xEEFC`・NEC 特殊文字 `0x8790-` の `≒ ≡ ∵` など）は、開いて保存すると触っていない行でも Windows が選ぶ方の符号に書き換わる（見た目の字は同じ・メモ帳と同じ振る舞いの読み・実測なし）。
- 壊れた UTF-8: BOM の無い UTF-8 に壊れたバイトがあると、CP932 として読めれば Shift_JIS で開く（文字化けで見え、ステータスバーに Shift_JIS と出る・保存すると CP932 で書き戻す）。

1.0.0（Store 版）で増えた物:

- F1 の「Windows の右クリックに登録する」は、packaged（Store 版）ではエクスプローラーから見えない見込み（読み・未実測。HKCU への書き込みがパッケージ専用の場所へ写る見込みで、製品は packaged かを見ていない）。PRIVACY と README は ZIP 版の動きを書き、Store 版は効かない見込みで未実測と書いた（#1656）。施主の packaged の確かめで見る。（追記 2026-11-17: 施主の確かめ 6-C-4 で Store 版ではエクスプローラーに出ないと確かめた。1.0.0 はこのまま提出し、次の版 1.1.0 から Store 版の F1 の一覧に 2 つの操作を出さない・#1809・ADR 0178 の追補）
- 中国語（簡体字）の画面の文は機械の訳で、母語の人のレビューは無い（`docs/i18n/REVIEW.md`）。
- WACK と packaged の起動はまだ（根拠 (d)）。AppxManifest の DisplayName と Description は英語の 1 つだけ（言語ごとの出し分けなし）・VERSIONINFO の CompanyName は空（REL-A の B・どちらも止めない）。

今日わかった物（どれも 1.0.0 を止めない）:

- 撮影の残りの指摘: 日本語と中国語の表示言語でも、Nib 自身の Ex の断りなどは英語のまま（#1585）。英語の表示言語で Ctrl+P の面の行が等幅の字体で描かれる。中国語の表示言語で、かなを含むファイル名が中国語の UI の字体で描かれる。知らせを出している間、エクスプローラーの縁がステータスバーの左に出る。ほか軽い物は 1.0.0 の後に Issue。
- ステータスバーは、入らないとき行と桁 → 表示形式の印 → 案内の順に落とす（#1658・ADR 0154 の追補）。最小の窓（360 DIP）では行と桁がほぼいつも落ち（96 / 120 DPI とも 165 形のうち 164・残るのは日本語の `通常` ＋ `行 17, 桁 4`）、狭い窓（96 DPI でおよそ 535〜610 px・V-1665 の読み）ではモードの文と知らせが変わるたびに行と桁が出たり消えたりする。
- **設定の互換**（V-1668 の「設定の互換」の節と後続 F1・`D:/NeNeNib/outputs/20261114/review-1668/review.md`・コードの読み）: 1.0.0 の設定の読み手は、知らない鍵のある `settings.v4` を `Failure::malformed` と読む。そのセッションは既定の設定（配色・字体など）で起動し、以後の設定の保存は全部拒む。ファイルは書き換えず消しもしない（壊さない・捨てない・保存は拒む）。だから後の版が v4 に鍵を足して書くと、1.0.0 に戻した人や、新しい版と 1.0.0 を同じ置き場で併用する人がこの状態になる。設計席の決め: 1.0.0 の読み手は ADR 0114 の厳密な読みのまま止めず、**1.0.0 の後は v4 に鍵を足さない**。鍵を足す PR（#1668 の `autoindent` が最初）は、施主に聞いてから「版を上げる（v5・旧版は残す）」か「知らない鍵を読み飛ばして残す読み手」を先に入れる。`explorer_width`（#1380）も同じ形で v4 に足されていて、どの公開版がそれを読めないかは公開の後に調べる。
- 複数のファイルをドロップすると、入力行の間・IME の変換中・Vim の待ち・Ctrl+Tab の歩きの間（と、裏で読む大きいファイルを 2 本以上落としたとき）は最後の 1 本だけ開き、知らせも出ない。失うのは開くはずのタブだけで、本文と未保存の物は失わない（V-1662 の後続 F1・`D:/NeNeNib/outputs/20261114/review-1662/review.md`・公開の後に Issue）。

作り直し 2 で足した物（2026-11-15・上の「作り直し 2」の「既知の限界に足す」と同じ。どれも 1.0.0 を止めない）:

- Shift_JIS の拡張文字（`0xED40-0xEEFC` 374 組・`0x8790-0x879C` 9 組・`0xFA4A-0xFA5B` 15 組の計 398 組）は、無編集で保存しても Windows の正準の符号に書き換わる（字は同じ・上の「Shift_JIS の二重の符号」を洗い出し H5 が実の CP932 で測って確かめた・#1690）。
- 改行の手前の literal CR と保存の改行の形の切り替えの組み合わせでは、バイト列が往復で変わる稀な形がある（#1690）。
- 自動退避の一覧の中身の版が 2 になった（#1688）。beta.13 までの Nib は版 2 の退避を問わず、消しもしない。文字コードや改行の形だけを選び直した文書は退避し直されず、落ちると古い形で戻る（本文は失わない・前から・#1704）。退避の store と古い残骸の forget の間に落ちると同じ文書が 2 本並びうる（#1705）。
- オペレータ待ちの `:s`: `gU` `gu` `g~` の起点が字の途中に落ちる形はキャレットが Vim と違う（本文は同じ）・`d` `c` `y` で字の途中に落ちると Vim は字を割り Nib はオペレータを捨てる・パターンの `\n`（#1566）・`d:s` の後の `u` が 2 回・レジスタの中の `:s` の行（#1701 #1696）。
- 矩形の INSERT: `$A` で動いた形・全角や Tab をまたいで動いた形・`<C-o>` の後（#1544）・Vim 自身が不正な UTF-8 を作る形は Vim と違う（どれも妥当な UTF-8）。`<Del>` で最後の行が無くなる形と `3I` のキャレット（#1706）・`c` `C` の後の `u`（#1707）。
- 保存の後始末: 1176 / 1177 は純関数の試験だけで守る・1176 の置き直しの後は属性と ACL を失いうる・1177 の後に OS が別の名前で残した元のファイルは回収しない（#1702）。

## 梱包

本番の物（公開元 `80fd7a5371de9f09bfacc12b7ed5f40ff13f6ba6`・作業木 `D:/NeNeNib/worktrees/v1.0.0`・置き場 `D:/NeNeNib/releases/v1.0.0/`）。値は席 REL-H の照合 `D:/NeNeNib/evidence/v1.0.0-release/final-verification.md`（2026-11-15）から写した（bytes と SHA-256 は `stat` と `sha256sum` で読み直してある）。上の「作り直し 2」の 2 つ目の公開元の物と、その前の物は上げていない。

| 物 | 名前 | bytes | SHA-256 |
| --- | --- | --- | --- |
| exe（`build/release-80fd7a5/NeNeNib.exe`） | `NeNeNib.exe` | 3,642,880 | `947abac9c162c65bc4048c1f3f18a1f0519500464ee371674cf6b024d27b6ef3` |
| ZIP | `NeNeNib-1.0.0-windows-x64.zip` | 1,507,213 | `a974fd61a2fedd005d4ecd4eb2d716990d2e7b5b53752913e4a5ee9c30274ac8` |
| ZIP の `SHA256SUMS` | `SHA256SUMS`（1 行・CRLF） | 97 | `ab1053ccd6a6c338088807cb57866f24c0e77eb29e143ac8896d8c8e1754d390` |
| MSIX（Store 用・署名なし） | `NeNeNib-1.0.0.0-windows-x64-store.msix` | 1,626,725 | `4f66f149f62de33a36b1ea378e1a6c3f5e0c3527f9d90398c5d36d1ebe85b59c` |
| MSIX の `SHA256SUMS` | `SHA256SUMS`（1 行・CRLF） | 106 | `933e18f83cb175066e73a974e61a425338ae563c2f0b374d856ee9b80d773043` |

照合:

- [x] `package.json` と `msix.json` の `commit` が公開元 `80fd7a5371de9f09bfacc12b7ed5f40ff13f6ba6`（`git rev-parse origin/main`・作業木の HEAD も同じで `git status --porcelain` 0 行）・`package.json` の `verification.path` がこの記録
- [x] `package.json` の `productVersion` と `version` が `1.0.0`、`metadata` の `fileVersion` `1.0.0.0` と `productVersion` `1.0.0`
- [x] `msix.json` の `executable.sha256` が ZIP の中の exe と同じ（`zip.executableMatches`）
- [x] `msix.json` の `packageVersion` が `1.0.0.0`・`store` が true
- [x] manifest の `Resources` が `en-US` `ja-JP` `zh-Hans` の順
- [x] `msix.json` の `entries` が 32 個（中身 30 個 ＝ exe・`LICENSE`・`THIRD_PARTY_NOTICES.txt`・`AppxManifest.xml`・`resources.pri`・`Assets` 25 枚、に makeappx が足す `AppxBlockMap.xml` と `[Content_Types].xml`）
- [x] 展開した exe の短い起動（隔離 profile）: 2026-11-15 15:05・`"result": "pass"`・exit 0・`documentUnchanged true`・結果 `D:/NeNeNib/evidence/v1.0.0-smoke-80fd7a53/result.json`（根拠の全文は `final-verification.md`）

## GitHub の公開

2026-11-17 20:53 JST（`publishedAt` 2026-11-17T11:53:23Z）に公開した（Store の認定の後・施主の決め 2026-11-15 18:32 のとおり）。

- tag: `v1.0.0` → 公開元 `80fd7a5371de9f09bfacc12b7ed5f40ff13f6ba6`
- Release: 題 `NeNe Nib 1.0.0 Beta`・pre-release・URL <https://github.com/hideyukiMORI/nene-nib/releases/tag/v1.0.0>・release id 448562273。公開の文は Store のリンク入り
- 添付: `NeNeNib-1.0.0-windows-x64.zip`（1,507,213 bytes・`a974fd61a2fedd005d4ecd4eb2d716990d2e7b5b53752913e4a5ee9c30274ac8`）と `SHA256SUMS`（97 bytes・`ab1053ccd6a6c338088807cb57866f24c0e77eb29e143ac8896d8c8e1754d390`）。上の「梱包」の表と同じ値
- 再取得の照合: `D:/NeNeNib/evidence/v1.0.0-release/verify-publication.py` → `result: pass`（取り寄せたバイト列が手元の本番と一致・ZIP の中身が一致・短い起動は 2026-11-15 15:05 の結果を使う）。出力 `D:/NeNeNib/outputs/20261117/verify-publication-v1.0.0.txt`・公開のログ `D:/NeNeNib/outputs/20261117/publish-v1.0.0.log`。1 回目は、先に `gh release download` をしていなくて止まった（手順書 README の 5 に書いてある）。取り寄せてから回し直して通った

## Microsoft Store の提出

提出した物は MSIX `NeNeNib-1.0.0.0-windows-x64-store.msix`（1,626,725 bytes・SHA-256 `4f66f149f62de33a36b1ea378e1a6c3f5e0c3527f9d90398c5d36d1ebe85b59c`・上の「梱包」と同じ）。

| 順 | 誰 | 状態 | 日時・記録 |
| --- | --- | --- | --- |
| 手順書 1〜9・WACK | hide | PASS | 2026-11-15（[quality-day-2026-11-15.md](quality-day-2026-11-15.md)） |
| 申請（Submission 1） | hide | 提出 | 2026-11-15 18:21（同上） |
| 認定・Store で公開 | Microsoft | 済 | 施主が 2026-11-17 の夜に「リリースされてる」と伝えた。認定と公開の日時は聞いていない（書かない） |

ページ: <https://apps.microsoft.com/detail/9nclgdtfq529>

## 公開の結果

- **公開版は 1.0.0**: Microsoft Store（認定されて公開済み・上のページ）と GitHub の Release `v1.0.0`（pre-release・2026-11-17 20:53 JST）。
- **公開元**は main `80fd7a5371de9f09bfacc12b7ed5f40ff13f6ba6`（上の「公開元」の表の 40 桁はこれ）。公開の後の照合は `result: pass`（上の「GitHub の公開」）。
- 公開元の後、main には 2026-11-15 夜から機能が入っている（凍結は解いた）。1.0.0 の物は公開元の commit から作った物で、main の今の機能は入っていない。
- 次: 次の版は 1.1.0 の予定で、まとめ枝 41 を区切りに Store を更新する（施主「41 で行こう」2026-11-17 夜）。
