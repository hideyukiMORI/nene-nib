# ADR 0154 — 文書の言語は文書の値で、表は core に 1 つ。設定とセッションには入れない

- 状態: 提案（hide の試用の後に受理）
- 日付: 2026-10-22
- Issue: #776
- 規則: ARC-001 / ARC-004 / ARC-007 / CPP-002 / CPP-005 / CPP-011 / CPP-012、ADR 0022（Ex の `:set`）/ 0056（文書の束）/ 0059（`UnloadedDocument`）/ 0066 / 0067（名前付き保存）/ 0070 / 0079（`write_status` の枠）/ 0140（表示形式は文書の値）

## 文脈

貼り付けの切り分け（#774）・Markdown のプレビュー（#763）・構文の色（S 段 1）は、どれも「この文書は何の言語か」で振る舞いを変える（hide の確認 2026-10-21 夜）。判定がそれぞれの所に散ると、拡張子の表が 3 つになり、`:set filetype=` で変えたときに一部だけが追いかける。

表示形式（ADR 0140）は文書ごとの値として `DocumentState` と `EditorState` のアクティブな文書の欄に置き、設定には入れない形がすでにある。言語も同じ形に置く。

## 決定

1. **言語は文書の値。** `core::Language` は閉じた enum（`plain` `markdown` `html` `c` `cpp` `python` `json` `javascript` `typescript` `yaml` `toml` `xml` `css` `powershell` `shell` `cmake`）。`DocumentState.language` と `EditorState`（アクティブな文書の `language()`）が `DisplayForm` と同じ所有で持ち、タブの切り替えの 1 か所（`parked_active` と `spread`）だけが置いて広げる（ADR 0056）。設定（`settings.v3`）とセッション（`session.v2`）には入れない。起動の復元は拡張子から引き直し、`:set filetype=` の上書きは窓を閉じると消える。
2. **表は core に 1 つ。** `language_of_path(std::string_view path) noexcept -> Language`（`LanguageTable.hpp`）は、パスの最後の区切り（`\` か `/`）の後ろの名前が `CMakeLists.txt` なら `cmake`、ほかは最後の `.` の後ろの拡張子を ASCII の大文字小文字を区別せずに `constexpr` の表で引き、無ければ `plain`。名前の無い文書・拡張子の無い名前も `plain`。初版の拡張子: `md markdown` → markdown・`html htm` → html・`c h` → c・`cpp cc cxx hpp hh hxx ipp` → cpp・`py pyi` → python・`json` → json・`js mjs cjs` → javascript・`ts mts cts` → typescript・`yml yaml` → yaml・`toml` → toml・`xml` → xml・`css` → css・`ps1 psm1 psd1` → powershell・`sh bash zsh` → shell・`cmake` → cmake・`txt` → plain。application はパスの `Document` から引く `language_of(const Document &)`（`Document.hpp`）だけを呼び、呼び出し元で拡張子を見ない。
3. **名前は Vim の filetype 名。** `language_name(Language) -> std::string_view` は `plain` が `"text"`、ほかは `"markdown"` `"html"` `"c"` `"cpp"` `"python"` `"json"` `"javascript"` `"typescript"` `"yaml"` `"toml"` `"xml"` `"css"` `"ps1"` `"sh"` `"cmake"`。逆引き `language_of_name(std::string_view) -> std::optional<Language>` は小文字の名前だけを受ける。名前の表は enum の宣言順の 1 つの配列で、2 本の関数と補完の候補（`all_language_names`）が同じ配列を読む。表の長さと enum の数は `static_assert` で揃える（`OperationTexts.cpp` と同じ形）。
4. **`:set filetype=<名前>` と `:set ft=<名前>`。** `:set display=` の隣（`ExResult.cpp` の設定に載せない option）で評価し、答えは `ExResult.language`（`ExLanguageRequest`・選んだ言語か、空なら問い）。名前を `language_of_name` で引けなければ **E474: Invalid argument**（`ExFailure::invalid_argument`。Vim は任意の文字列を受けるが Nib の言語は閉じているので断る）。`:set filetype?` `:set ft?` は今の言語を `filetype=<名前>` の 1 行で知らせ、選んだときも同じ 1 行を出す（controller の `answer_language`）。controller は意図 `SelectLanguage`（`SelectDisplay` と同じ形）で文書の値を置く。使う人の操作なので `keeps_tab_walk` と `begin_intent` の表には載せない。補完の候補の表に `set filetype=` と `set ft=` を足し、`=` まで打ったときだけ言語の名前を `set ft=<名前>` の形で表の順に足す（字体の候補と同じ形。全件の一覧には命令の頭だけ）。
5. **いつ決まるか。** 開くとき（`open_document` の 1 本・`EditorState::with_opened` がパスから引く）、名前が変わる保存（`:saveas` / 無題の `:w file` / Ctrl+Shift+S で新しいパスになったとき・`with_document` がパスの変化を見て引き直し `:set filetype=` の上書きは捨てる。同じパスの保存と、名前を変えない `:w file` の写しは保つ）、`:set filetype=`。`UnloadedDocument`（まだ読んでいないタブ）は `reach_tab` で読んだ束（`reached_bundle`）でパスから決める。自動退避から戻した束もパスから引く。NewTab（名前なし）は `plain`。
6. **ステータスバーの右側の 4 項目目は右端。** `status_items_for(caret, encoding, ending, language)` が言語の名前（`language_name`）を最後に足し、`status_item_count` は 4。並びは左から「位置・文字コード・改行・言語」で、既存の 3 項目の順は変えない（VS Code と同じく言語は右端）。`StatusBarLayout` の項目の幅は 96 / 72 / 36 / 72 DIP（言語は最も長い `javascript` `typescript` が入る幅）で、右寄せの既存の規則のまま右から並べる。`write_status` の保持の枠は 10 → 11（通常 7 枠と案内 4 枠・録画中は 8・ADR 0079 の決定 11 の追補）。保持の外で毎 frame 文字組みを作る形（表示形式の印の `write_right`）は打鍵のたびに layout を作るので採らない。右の項目が 88 DIP 広がるので、ステータスの案内（ADR 0079 の決定 9）と `:` の入力の欄はその分だけ狭くなる。右の 4 項目を置いて `:` の入力欄の右端（最初の項目の左から 16 DIP）が窓の左から `command_input_minimum_dip`（96 DIP・`CommandLayout.hpp`）に届かない窓では、`StatusBarLayout` の 1 か所が言語の欄を畳み（右端の幅 0 の矩形・ほかの 3 項目は言語が無いときの位置・renderer は幅 0 の欄を書かず枠も使わない）、案内を消す規則（ADR 0079 の決定 9）はその後の配置で判断する（96 DPI では 448 DIP 未満で畳む・最小の 360 DIP で入力欄の右端は 96 DIP）。
7. **ほかの所は言語を読むだけ。** 貼り付け（#774）・プレビュー（#763）・構文の色（S 段 1）は `EditorState::language()` / `DocumentState.language` を読む。判定の表を別に作らない。
8. **限界。** shebang・modeline・中身からの判定は無い。`.txt` と拡張子なしは `plain`。言語の一覧は閉じている（増やすのは enum と名前の表と拡張子の表の各 1 行）。`:set filetype=` は Vim と違い閉じた名前の外を E474 で断る。`:set filetype`（`?` なし）の問いの形は受けない。`:set ft=`（空）も Vim は filetype を空にするが、Nib は閉じた名前だけを受けるので E474。

## 強制

- `Language` の値が増えたら `is_language` の `switch` が落ち（CPP-002）、名前の表の行が無ければ `static_assert` が落ちる。
- 意図 `SelectLanguage` は `EditorIntent` の閉じた和型に入り、`std::visit` の写し先が無ければコンパイルが落ちる（ADR 0009）。
- scope `--language`（`tests/unit/LanguageTests.cpp`）: 拡張子の表（大文字小文字・区切り・`CMakeLists.txt`・拡張子なし）・名前の往復 16 本と小文字だけ・Ex の評価と E474・補完・開く / `:set ft=` / NewTab / 切り替え / 写しの `:w file` / `:saveas` / 同じパスの `:w`・まだ読んでいない文書を読んだとき・設定に書かないこと・ステータスの 4 項目目と配置・狭い窓で言語の欄を畳むこと（360 / 447 / 448 / 800 DIP）・閉じて隣へ移るとき・自動退避から戻した束。

## 結果

- 言語で振る舞いを変える所は、文書の値を 1 つ読むだけになる。表を足すのは core の 1 か所。
- 言語は保存されないので、`:set filetype=` の選び直しは窓を閉じると消える（拡張子から引き直す）。

## 追補（2026-10-22・#855）

- **言語を 3 つ足す（決定 1・2・3 の表の各 1 行）。** `Language` の末尾に `gdscript` `dosini` `gdresource`（名前は Vim 9.1 の filetype 名）を足し、拡張子は Vim の `filetype.vim` に寄せて `gd` → gdscript・`ini` → dosini・`tscn` `tres` → gdresource。`godot`（`project.godot`）は Vim に行が無いが同じ書式なので gdresource。`cfg` は Vim では別の filetype（`cfg`・中身で `rapid` も）なので表に入れず plain のまま（Issue #855 の初めの案と違う所）。名前の往復は 19 本。貼り付けの切り分け（`paste_kind_of`）では 3 つとも text。

## 追補（2026-10-22・#857）

- **言語を 3 つ足す（決定 1・2・3 の表の各 1 行）。** `Language` の末尾に `cs` `rust` `go`（名前は Vim 9.1 の filetype 名）を足し、拡張子は Vim の `filetype.vim`（`dist/ft.vim` の拡張子の表）に寄せて `cs` `csx` `cake` → cs・`rs` → rust・`go` → go（`csx` `cake` は Issue #857 の初めの案に無く Vim に寄せて足した所・`go.mod` は Vim では別の filetype なので plain のまま）。名前の往復は 22 本。貼り付けの切り分け（`paste_kind_of`）では 3 つとも text。`:set ft=` の名前の外の例（試験）は `rust` から `zig` に替えた。

## 追補（2026-10-22・#860）

- **言語を 4 つ足す（決定 1・2・3 の表の各 1 行）。** `Language` の末尾に `php` `sql` `lua` `dockerfile`（名前は Vim 9.1 の filetype 名）を足し、拡張子は Vim の `filetype.vim` に寄せて `php` `php0`〜`php9`（Vim の `*.php\d`）`phtml` `ctp` `phpt` `theme` → php・`sql` → sql（Vim は `g:filetype_sql` が無ければ sql）・`lua` `rockspec` → lua・`dockerfile`（`*.[dD]ockerfile`）→ dockerfile。名前の行（`CMakeLists.txt` と同じ道・大小を問わない）に `Dockerfile` `Containerfile` → dockerfile と `.luacheckrc` → lua を足し、Vim の最後の節の `Dockerfile.*` `Containerfile.*` は拡張子の表に当たらなかった名前だけ dockerfile にする（`Dockerfile.json` は json・`Dockerfile.txt` は plain・`Dockerfile.dev` は dockerfile）。`luau` は Vim では別の filetype なので plain のまま。名前の往復は 26 本。貼り付けの切り分け（`paste_kind_of`）では 4 つとも text。

## 追補（2026-10-23・#864）

- **字句器のある言語に拡張子と名前の行を足す（新しい `Language` は無し）。** 拡張子: `tsx` → typescript・`jsx` → javascript（Vim 9.1 は `typescriptreact` `javascriptreact` の別の filetype だが、JSX のタグの色は埋め込みの設計を待つ後続なので親の言語に畳む）・`pssc` → powershell（Vim と同じ ps1）・`import` → gdresource（Godot の `.import` は ConfigFile の書式・`godot` と同じく Vim に行は無い）。名前の行（`CMakeLists.txt` と同じ道）: `.bashrc` `.bash_profile` `.bash_logout` `.profile` `.zshrc` `.zprofile` → shell（Vim は bash / sh / zsh に分けるが、`zsh` の拡張子と同じく shell の 1 つに畳む）。名前の末尾の照合を 1 本足し、`*.cmake.in` → cmake（Vim と同じ・拡張子の表は 2 段にしない）。名前の往復は 26 本のまま。

## 追補（2026-10-28・#849）

- **ファイルを選ぶ窓の絞り込みも同じ表から作る（決定 2・3）。** core の `listed_language_extensions()` が拡張子の表の拡張子だけを表の順に重複なく並べ（`CMakeLists.txt` などの名前の行と `*.cmake.in` の末尾の照合は含めない）、`file_dialog_pattern()` がそれを `*.md;*.markdown;…;*.txt` の形でつなぐ。plain の `txt` は表の行にあり、無くなれば `static_assert` が落ちる（拡張子の重複も `static_assert` が落とす）。ui の `FileDialog.cpp` は拡張子を書かず、開くと保存の 2 つのダイアログに「対応するファイル」（core の pattern を `to_utf16`）と「すべてのファイル」（`*.*`）を渡し、既定は「すべてのファイル」（`SetFileTypeIndex(2)`）。保存の既定の拡張子 `txt` は変えない。表に拡張子を足すと絞り込みにも載る。契約は scope `--language` の core の関数の試験で、実機のダイアログは出さない。

## 追補（2026-11-14・#1658）: モードの文と行と桁・表示形式の印・案内は、測った幅で落とす順を決める

1.0.0 の前の撮影で、最小の窓（360 × 200 DIP）の英語の `Basic` と `Ln 17, Col 4` が重なり、中国語の `标准` と `行 17，列 4` の間が無かった。モードの枠（72 DIP）と行と桁の枠（96 DIP）は最小の窓でもともと重なっていて（96 DPI で [123, 195] と [112, 208] の物理画素）、どちらの文も枠で切らず、行と桁は右寄せで枠より広ければ左へはみ出していた。決定 6 の畳む規則（言語の欄 → 診断の件数）は文の幅を見ないので、これを拾えなかった。

1. **落とす順は core の 2 本。** `status_bar_fitted(layout, dpi, mode_width, position_width)` と `status_guide_fitted(guide, status, dpi, mode_width)`（`OperationGuideLayout.hpp`）。幅は ui が描画と同じ書式で測った文の幅（保持している `write_status` の枠の layout の `GetMetrics` だけ・frame ごとの `CreateTextLayout` も確保も足さない・枠の数は最大 11 のまま）。モードの文の右端から `status_text_gap_dips`（6 DIP・96 DPI で 6 px・120 DPI で 8 px）を空けて始まらないものを落とす。順は (1) 行と桁（枠より広い文は枠を左へ文の幅まで広げてから判断）→ (2) 表示形式の印（行と桁の左隣に置き直し、落としたら空いた所へ寄せて入るかを見る）→ (3) ステータスの案内（左端が届かなければ出さない）。言語の欄・件数・文字コード・改行は決定 6 のまま（文の幅で動かさない）。
2. **収まるときは今の配置のまま。** どちらの文も枠に入り間が空けば `fitted == layout` で、字の位置は 1 px も変わらない。行と桁の文は今までと同じく測らない枠に右寄せで書き、広げた枠は印と録画の欄の右端だけに効く。
3. **当たりは測らない配置を読む。** `status_bar_hit` が拾うのはトグルと文字コードと改行だけで、落とす 3 つ（行と桁・印・案内）はどれも押せない。見える所と押せる所は食い違わない。`:` の入力と知らせの frame はモードを描かないので測らない配置のまま。
4. **録画の欄は印の手前まで。** `recording @a` は今までどおりモードの文の 12 DIP 後ろから行と桁の左まで（測った配置）で切り、表示形式の印を書く frame は印の左まで（案内は録画中に出ない・`guide_context`）。
5. **実測（`nib_window_tests --language`・Segoe UI Variable Text 12 DIP・モードは太字）。** 測った集合は 3 言語 × モードの文 11 個（`mode_label` の全部: 通常と Vim の 5 モード × INSERT の `Ctrl-o` の待ちの有無）× 行と桁 5 組（`17, 4`・`9999, 999`・`12345, 123`・`1073741824, 1`・`1, 1073741824`）× 96 / 120 DPI × 窓 4 通り（最小・案内が出る最も狭い窓・1280 px・1920 px）の 1,320 形。
   - モードの文の幅（96 DPI）は `NORMAL` 53 px・`VISUAL BLOCK` 83 px・`(insert) VISUAL LINE` 115 px・`(insert) VISUAL BLOCK` 127 px で、72 DIP の枠を超える文がある。行と桁は `行 17, 桁 4` 54 px・`Ln 17, Col 4` 60 px・`Ln 12345, Col 123` 91 px で枠（96 px）に入り、枠を超えるのは 10 桁の `1073741824` の組だけ（102〜108 px・120 DPI は 128〜135 px で枠は 120 px）。
   - 最小の窓では行と桁を 96 DPI と 120 DPI でそれぞれ 165 形のうち 164 形落とす。残るのは日本語の `通常` と `行 17, 桁 4` だけで、間は両 DPI とも 8 px（直す前と同じ位置）。直す前は、英語の `Basic`（3 px）と Vim のどのモードの文でも実際に重なり（日本語の Vim の `NORMAL` と `行 17, 桁 4` は 21 px・重なっていた形は両 DPI で 163 形ずつ）、中国語の `标准` と `行 17，列 4` は 96 DPI で 1 px・120 DPI で 0 px しか空いていなかった（撮影で「間が無い」とされた形で、6 DIP に足りないので落とす）。間を 6 DIP にしたのは、直す前に 8 px 空いて出ていた日本語の `通常` を 120 DPI でも落とさないため（8 DIP は 120 DPI で 10 px になり落とす）。表示した行と桁とモードの文の間は、どの形でも 8 px 以上（12 DIP の字の大きさの 2/3 em（96 DPI）と 1/2 em（120 DPI）・Segoe UI の空白の約 2 つ分）。
   - 1280 px と 1920 px の窓では行と桁も印も落ちず、4 桁と 3 桁までの組は 3 言語 × 11 個のどれでも測らない配置のまま（`fitted == layout`）。
   - 案内は 96 DPI で 734 px、120 DPI で 919 px の窓から出て、その左端は 222 px / 279 px。`(insert) VISUAL LINE` と `(insert) VISUAL BLOCK` の文はこれに届く（96 DPI で 755 px / 767 px 未満、120 DPI で 946 px / 961 px 未満の窓）ので、その帯の窓でその 2 つのモードの間だけ案内を出さない。ほかのモードの文は案内に届かない（`VISUAL BLOCK` は 96 DPI で 11 px 空く）。
   - 直す前の答え（2 本とも `layout` / `guide` をそのまま返す）では、この試験（上の 1,320 形）は 1,246 件落ちる（行と桁がモードの文から間を空けない 328・印が間を空けない 330・行と桁が印の欄にかかる 528・案内がモードの文に届く 60・6 DIP で測り直した値）。unit の `--guide` は 2 件、`--command-palette` は鍵の欄の 6 件落ちる。
