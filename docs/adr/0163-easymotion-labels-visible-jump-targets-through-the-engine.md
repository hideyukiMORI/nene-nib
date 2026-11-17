# ADR 0163 — イージーモーションは engine の状態機械で、見えている跳び先に印を出し、印の鍵で跳ぶ

- 状態: 受理（施主決定 D67・FR-023・2026-10-28「イージーモーション推しのままでおけ」。実装は M2〜M4 の席・受理は D41 の形）
- 日付: 2026-10-28
- Issue: #998（親）・計画 `docs/plans/2026-10-28-easymotion.md`
- 規則: FR-023、ARC-001 / ARC-003 / ARC-004 / ARC-011、CPP-002 / CPP-005 / CPP-011 / CPP-012、QLT-014、ADR 0012（oracle・本機能は oracle 不能）/ 0015（語の分類）/ 0030（`.`）/ 0037（当たりの枠）/ 0041（engine の外の preview）/ 0049（Space は `l`）/ 0052（`''`・ジャンプ）/ 0053（1 文字）/ 0061（IME の構え）/ 0129 / 0144（表示領域の問い）/ 0152（マーク）

## 文脈

vim-easymotion（`<Leader><Leader>w` など）は Vim / Neovim で最も使われる移動の plugin の 1 つで、見えている範囲の跳び先に 1〜2 文字の印を重ね、印の鍵でそこへ跳ぶ。Nib の Vim は engine が純関数で、鍵の表（`VimKeyTable`）・接頭辞（`VimPrefix`）・1 鍵の待ち（`VimInputWait`）・表示領域の問い（`VimEditorView`）・当たりの面の描画が揃っている。本物の Vim には無い機能なので oracle の fixture は作れず（ADR 0012 の外）、契約と性質の試験で守る。hide の要望は 2 バイト文字（日本語）への対応。

## 決定

1. **鍵は `\\`（バックスラッシュ 2 回）の後に動作。** `VimPrefix::leader`（1 回目）→ `VimPrefix::leader_leader`（2 回目）→ 動作の 1 鍵。NORMAL と VISUAL（3 種）。表に無い鍵はビープして何もしない（保留を捨てる）。回数は受けない（回数の後の `\` は回数を捨てる）。INSERT では `\` は文字。
2. **動作は 9 つ（閉じた enum `EasyMotionKind`）。** `w`（語の頭・前）`b`（語の頭・後）`e`（語の尾・前）`j`（下の行の最初の非空白）`k`（上の行）`f{文字}`（前の文字）`F{文字}`（後の文字）`s{文字}`（前後両方）`l`（見えている全部の語の頭）。語の切れ目は `w` と同じ語の分類（`VimWordMotion` の表・漢字とかなの切れ目で日本語の中にも出る）。`f` `F` `s` は文字の待ち（決定 4）。
3. **跳び先と印は core の純関数。** `easymotion_targets(const VimEditorView &, EasyMotionKind, std::optional<char32_t>) -> std::vector<Offset>`（見えている行の範囲＝`view.viewport` の最初の行から見えている行数ぶん・本文の順・キャレットの位置は除く・`w` `e` `f` `s` の前は キャレットの後ろから、`b` `F` の後ろはキャレットの前まで・`s` と `l` は両方）。`easymotion_labels(std::size_t count) -> std::vector<std::string>`（印の文字 `asdghklqwertyuiopzxcvbnmfj;`・27 以下なら 1 文字・超えたら vim-easymotion と同じ組の割り当て: 先頭の文字が組の鍵で 2 文字目で選ぶ・どの印も他の印の接頭辞にならない・決定的）。跳び先が 0 ならビープ。跳び先が 1 つなら印を出さず即跳ぶ（vim-easymotion の既定）。
4. **状態は `VimState.easy_motion`（命令の途中の値）。** `std::optional<EasyMotionState{EasyMotionKind kind; std::vector<Offset> targets; std::vector<std::string> labels; std::string typed;}>`。`vim_resting_from` で消える側（`selected_register` と同じ）。`f` `F` `s` の文字の待ちは `VimInputWait` に `EasyMotionKind` を足した値で、controller はこの待ちのとき **IME を開いた 1 文字の入力行**（`CommandInput` の 3 つ目の種類・`/` の行と同じ場所・`ImeStance::as_left`）を出し、確定した文字列の最初の 1 文字（ADR 0053 の 1 文字・結合文字は含む）を鍵として engine へ流す（`vim_step` の `VimCharacter`）。ASCII は IME を経ずそのまま。Esc は取消。
5. **印の鍵の 1 鍵ごとに engine が絞る。** `easymotion_step(state, key)`: 打った文字が `typed` に続く印の接頭辞なら絞る（1 文字の印が確定したら跳ぶ）・どの印にも当たらなければ取消（ビープ・何も変えない）・Esc は取消。跳びは既存の `VimMoveTo` の効果で、**ジャンプとして `''` を置く**（ADR 0152 の `jumped_by` の道・`G` と同じ）。VISUAL では選択を伸ばす（既存の移動と同じ）。`.` は記録しない（移動なので）。
6. **印は `LineView` に載せて renderer が描く。** `LineView.labels`（`std::vector<core::MotionLabel{Column column; std::string label; std::uint8_t cells;}>`・見えている行だけ・`colors` と同じ桁）。renderer は字形の後に `accent` の地の矩形（幅 = `cells` × 升・升は `column_advance` の表・全角の跳び先は 2 升・半角で印が 2 文字なら次の升へはみ出す）に `on_accent` の字で描く。色のトークンは足さない。印が出ている間もキャレットは描く。格子表示（巨大な行）では印を出さない（決定 3 の `targets` が空）。
7. **費用。** 印が無いときの打鍵の道は `\` の 1 回目で表を 1 回引くだけ（分岐を足さない）。跳び先の探索は見えている行だけ（60 行 × 100 文字で数十 µs の見込み・任意のベンチ `easymotion-labels-visible` を足して PR 本文に書く・D67 の +1% の線は印が無いときの 1 打鍵に適用）。
8. **契約。** core: `--vim-easymotion`（9 動作の跳び先・日本語の語の切れ目・キャレットの除外・1 つなら即跳ぶ・0 ならビープ・印の割り当て・絞りと取消・`''`）と性質（固定 seed のランダムな本文で `targets` が本文の順で重複なし・`labels` が重複なく接頭辞が他の印と重ならず `count` と同じ数・`step` の全経路が「絞る / 跳ぶ / 取消」のどれかに落ちる）。application: `--vim-easymotion-input`（`\\f` の入力行・IME の確定の 1 文字・Esc・VISUAL の伸び・`.` に載らない・`LineView.labels` の写し）。ui: `nib_window_tests` の印の画素（WIC）と `eng/verify-window.py --easymotion`。fuzz: `eng/vim-fuzz.py` の語には**載せない**（oracle 不能）。
9. **限界（後続）。** operator-pending（`d\\w`）・`t` `T` `n` `N` `L`（行の末尾）・折り返した行の画面の外の部分（論理行が見えていれば対象に入る）・2 枠（split）のもう 1 つの枠・回数・印の文字の設定。

## 強制

- engine が純関数・OS に触れない: **active**（`eng/symbols.py`）。
- 印が無いときの打鍵の道の不変と 8 本の速さ: **active**（QLT-014・A/B は PR 本文）。
- 契約と性質の試験が既定実行に載る: core の分（`--vim-easymotion`・`--vim-easymotion-property`）は **active**（M2・#1774・`tests/unit/NibTests.cpp` の `contracts`）。application の分（`--vim-easymotion-input`）も **active**（M3・#1779）。ui の印の画素は `nib_window_tests --easymotion`（M4・#1782・窓も device も作らない組）で、実機の道具は `eng/verify-window.py --easymotion`（窓を最前面に出すので設計席が回す・回るまで実機の確かめは **planned**）。
- oracle の fixture: **不能**（本物の Vim に無い）。

## 結果

得られるもの: 見えている範囲のどこへでも 3〜4 打鍵で跳べる。日本語の文の中の語の頭にも印が出る。`\\f` で日本語の文字へも跳べる。
失うもの・残る穴: `\` を 1 回目の接頭辞に使うので、将来 `\` 単独の鍵は置けない（Vim の既定で `\` は何もしない）。印が半角の字で 2 文字のときは次の字が隠れる（vim-easymotion と同じ）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| `s`（vim-sneak 風） | `s`（1 文字の置換・#768）と衝突 |
| `<Space><Space>` | Nib の Space は行をまたぐ `l`（ADR 0049） |
| 印を本文に差し込んで描く（IME の差し込みの道） | 本文の桁がずれ、文字組みを作り直す。字形の上に重ねる方が layout を保てる |
| 文字の待ちを IME 無しの 1 鍵に | 日本語の文字へ跳べない（hide の 2 バイト対応の要望） |

## 追補（2026-11-17・#1774）: 印の待ち・近い順・当たらなかった鍵

設計席の決め（M2 の実装とレビュー V-1777 の後）。

1. **印を出している間の待ちも `input_wait` に置く（決定 4 の拡張）。** `f` `F` `s` の文字の待ちだけでなく、印を出している間も `input_wait` は `EasyMotionKind` のままで、`VimState.easy_motion` が値を持つ。`easy_motion` が無ければ文字の待ち（M3 の controller は IME の 1 文字の入力行を出す）、あれば印の鍵。鍵の道は既存の `input_wait` の分岐で振り分けるので、印が無いときの打鍵の道に分岐を足さない（決定 7）。`map` の表と回数の桁は待ちの間は引かない（既存の道）。
2. **印はキャレットから近い順に付ける（決定 3 の補い）。** `easymotion_targets` は跳び先を近い順に返し、印は `easymotion_labels` の順にその位置へ付く（`labels[i]` が `targets[i]` の印・最初の印 `a` がいちばん近い跳び先）。一方向の動作は本文の上の近い順で、`w` `e` `j` `f` は本文の順、`b` `k` `F` は本文の逆の順（キャレットのすぐ前から後ろへ）。前後両方の `s` `l` だけ（行の差・仮想桁の差）の辞書順（バイトの差は日本語を遠く数えるので使わない）で、同じならキャレットより後ろの跳び先を先にする。27 × 27 を超えたら、並べてから切る。決定 3 の「本文の順」はこれで置き換える。
3. **`\` と `\\` の後の表に無い鍵は、普通の鍵として流し直す（決定 1 を変える）。** 待ち・回数・レジスタの名指しを捨てて鳴らし、その鍵を普通の鍵として処理し直す（本物の Vim の `map` の待ちが当たらなかったときと同じ・`\` を表に入れる前の振る舞いと同じ）。NORMAL の `\x` は `x` が効き、`v\x` は選択を消し、`v\<Esc>` は VISUAL を出る。Esc は鳴らさない Esc として流す。`d\\w` は `d\` でオペレータを捨てて鳴り、`\w` の `w` が普通の移動として効く。文字の待ち（`\\f` の後）と印の鍵の取消は決定 5 のまま（鍵を流し直さない）。`\` の 1 回目で回数とレジスタの名指しを捨て、`\` `\\` の待ちの鍵は `.` の記録に積まない（流し直した鍵だけが記録になる・`\x` の `.` は `x` と同じで `3.` の回数も効く）。
